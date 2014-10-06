export script_name        = "OptimASS"
export script_description = "Allow me to optimize your ass."
export script_author      = "torque"
export script_version     = 0x000003

ffi = require 'ffi'
log = require 'a-mo.log'

ffi.cdef [[
uint32_t optimASS_getVersion( void );
int      optimASS_init( int, int );
void     optimASS_addHeader( const char*, unsigned int );
void     optimASS_initEvents( unsigned int );
void     optimASS_addEvent( const char*, unsigned int, unsigned int );
int      optimASS_checkLine( const int, const int*, const unsigned int, uint8_t* );
void     optimASS_cleanup( void );
]]

optimASS = ffi.load aegisub.decode_path "?user/automation/include/OptimASS/libOptimASS" .. ('OSX' == ffi.os and '.dylib' or 'Windows' == ffi.os and '.dll' or '.so')

aegisub.register_macro script_name, script_description, ( subtitle, selectedLines, activeLine ) ->
	if optimASS.optimASS_getVersion! < script_version
		log.windowError "Installed libOptimASS is outdated. Please update it."
		return selectedLines
	elseif optimASS.optimASS_getVersion! > script_version
		log.windowError "Installed optimASS.moon is outdated. Please update it."
		return selectedLines

	local eventOffset, resX, resY

	scriptHeader = {
		"[Script Info]"
	}

	infoFields = {
		PlayResX: true
		PlayResY: true
		ScaledBorderAndShadow: true
		ScriptType: true
		WrapStyle: true
	}

	seenStyles = false
	seenDialogue = false

	subtitleLen = #subtitle

	for index = 1, subtitleLen
		with line = subtitle[index]
			if "info" == .class
				if infoFields[.key]
					table.insert scriptHeader, .raw

				if "PlayResX" == .key
					resX = tonumber .value
				elseif "PlayResY" == .key
					resY = tonumber .value

			elseif "style" == .class
				unless seenStyles
					table.insert scriptHeader, "[V4+ Styles]"
					seenStyles = true

				table.insert scriptHeader, .raw

			elseif "dialogue" == .class
				unless seenDialogue
					eventOffset = index
					table.insert scriptHeader, "[Events]"
					-- Since lua already has the lengths of the lines calculated,
					-- it seems like it would be a waste to re-re-calculate them
					-- in C.
					optimASS.optimASS_initEvents subtitleLen - eventOffset + 1
					seenDialogue = true

				optimASS.optimASS_addEvent .raw, #.raw, index - eventOffset

	vidResX, vidResY = aegisub.video_size!

	-- If a video is loaded, use its resolution instead of the script's
	-- resolution.
	if 0 != optimASS.optimASS_init vidResX or resX or 640, vidResY or resY or 480
		log.windowError "optimASS C library init failed."

	minimalHeader = table.concat( scriptHeader, '\n' ) .. '\n'
	optimASS.optimASS_addHeader minimalHeader, #minimalHeader

	ffms = aegisub.frame_from_ms
	msff = aegisub.ms_from_frame
	indicesToDelete = { }
	for eventIndex = eventOffset, subtitleLen
		with event = subtitle[eventIndex]
			unless .comment
				index = eventIndex - eventOffset
				humanIndex = index + 1

				if "" == .text
					log.debug "Line #{humanIndex} has no text and will be removed."
					table.insert indicesToDelete, eventIndex
					continue

				startFrame = ffms .start_time
				numFrames  = ffms(.end_time) - startFrame

				times = ffi.new "int[?]", numFrames
				-- ffi initializes arrays to 0
				results = ffi.new "uint8_t[?]", numFrames + 1

				for frame = 0, numFrames-1
					times[frame] = math.floor 0.5*(msff(startFrame + frame) + msff(startFrame + frame + 1))

				if 0 != optimASS.optimASS_checkLine index, times, numFrames, results
					log.warn "Line #{humanIndex} could not be rendered. Skipping."
					continue

				if 0 == results[numFrames]
					log.debug "Line #{humanIndex} is invisible for its duration and will be removed."
					table.insert indicesToDelete, eventIndex
				else
					for resIdx = 0, numFrames-1
						if 0 == results[resIdx]
							log.debug "Line #{humanIndex} is invisible on frame #{startFrame+resIdx}."

	subtitle.delete indicesToDelete

	optimASS.optimASS_cleanup!
