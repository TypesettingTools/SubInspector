export script_name        = "OptimASS"
export script_description = "Allow me to optimize your ass."
export script_author      = "torque"
export script_version     = "0.0.2"

ffi = require 'ffi'
log = require 'a-mo.log'

ffi.cdef [[
int  optimASS_init( int, int );
void optimASS_addHeader( const char*, unsigned int );
void optimASS_initEvents( unsigned int );
void optimASS_addEvent( const char*, unsigned int, unsigned int );
int  optimASS_checkLine( const int, const int*, const unsigned int, uint8_t* );
void optimASS_cleanup( void );
]]

optimASS = ffi.load aegisub.decode_path "?user/automation/include/OptimASS/libOptimASS" .. (('OSX' == ffi.os) and '.dylib' or ('Windows' == ffi.os) and '.dll' or '.so')

getDirty = ( subtitle, selectedLines, activeLine ) ->
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
					eventCount = subtitleLen - eventOffset + 1
					optimASS.optimASS_initEvents eventCount
					seenDialogue = true

				arrayIndex = index - eventOffset
				optimASS.optimASS_addEvent .raw, #.raw, arrayIndex

	vidResX, vidResY = aegisub.video_size!

	-- If a video is loaded, use its resolution instead of the script's
	-- resolution.
	resX = vidResX or resX or 640
	resY = vidResY or resY or 480
	result = optimASS.optimASS_init resX, resY
	if result != 0
		log.windowError "optimASS C library init failed."

	minimalHeader = table.concat( scriptHeader, '\n' ) .. '\n'
	minimalHeaderLength = #minimalHeader
	optimASS.optimASS_addHeader minimalHeader, minimalHeaderLength

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
				endFrame   = ffms .end_time
				numFrames  = endFrame - startFrame

				times = ffi.new "int[?]", numFrames
				-- ffi initializes arrays to 0
				results = ffi.new "uint8_t[?]", numFrames + 1

				for frame = 0, numFrames-1
					frameTime = msff startFrame + frame
					nextFrameTime = msff startFrame + frame + 1
					times[frame] = math.floor 0.5*(frameTime + nextFrameTime)

				status = optimASS.optimASS_checkLine index, times, numFrames, results
				if status != 0
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

aegisub.register_macro "Optimize Script", "Get your ass optimized", getDirty
