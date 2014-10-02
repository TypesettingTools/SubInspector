export script_name        = "OptimASS"
export script_description = "Allow me to optimize your ass."
export script_author      = "torque"
export script_version     = "0.0.1"

ffi = require 'ffi'
log = require 'a-mo.log'

ffi.cdef [[
int  optimASS_init( int, int );
void optimASS_addHeader( char*, unsigned int );
void optimASS_addEvents( char**, unsigned int*, unsigned int );
uint8_t* optimASS_checkLine( const int, const int*, const unsigned int );
void optimASS_cleanup( void );
]]

optimASS = ffi.load aegisub.decode_path "?user/automation/include/OptimASS/libOptimASS" .. (('OSX' == ffi.os) and '.dylib' or ('Windows' == ffi.os) and '.dll' or '.so')

getDirty = ( subtitle, selectedLines, activeLine ) ->
	local eventOffset, resX, resY, lineLengths, lines, eventCount

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
					lineLengths = ffi.new "unsigned int[?]", eventCount
					lines = ffi.new "char*[?]", eventCount
					seenDialogue = true

				arrayIndex = index - eventOffset
				lineLengths[arrayIndex] = ffi.cast "unsigned int", #.raw
				lines[arrayIndex] = ffi.cast "char*", .raw

	vidResX, vidResY = aegisub.video_size!

	-- If a video is loaded, use its resolution instead of the script's
	-- resolution.
	resX = vidResX or resX
	resY = vidResY or resY

	minimalHeader = table.concat( scriptHeader, '\n' ) .. '\n'
	minimalHeaderLength = #minimalHeader

	minHead_char = ffi.cast "char *", minimalHeader
	minHeadLen_uint = ffi.cast "unsigned int", minimalHeaderLength

	result = optimASS.optimASS_init ffi.cast( "unsigned int", resX ), ffi.cast( "int", resY )
	if result != 0
		log.windowError "optimASS C library init failed."

	optimASS.optimASS_addHeader minHead_char, minHeadLen_uint
	optimASS.optimASS_addEvents lines, lineLengths, ffi.cast( "unsigned int", eventCount )

	ffms = aegisub.frame_from_ms
	msff = aegisub.ms_from_frame
	for eventIndex = eventOffset, subtitleLen
		index = eventIndex - eventOffset
		with event = subtitle[eventIndex]
			unless .comment
				startFrame = ffms .start_time
				endFrame   = ffms .end_time
				numFrames  = endFrame - startFrame

				times = ffi.new "int[?]", numFrames

				for frame = 0, numFrames-1
					frameTime = msff startFrame + frame
					nextFrameTime = msff startFrame + frame + 1
					times[frame] = ffi.cast "int", math.floor 0.5*(frameTime + nextFrameTime)

				results = optimASS.optimASS_checkLine ffi.cast( "int", index ), times, ffi.cast( "unsigned int", numFrames )

				for resIdx = 0, numFrames - 1
					if 0 == tonumber results[resIdx]
						log.warn "Line #{index+1} is invisible on frame #{startFrame+resIdx}."

	optimASS.optimASS_cleanup!

aegisub.register_macro "Optimize Script", "Get your ass optimized", getDirty
