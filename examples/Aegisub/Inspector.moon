-- This library is unlicensed under CC0

export script_name        = "ASSInspector Example"
export script_description = "Calculates bounding rectangles on the start and end times of selected events."
export script_author      = "torque"
export script_version     = 0x000100

ffi  = require( 'ffi' )

ffi.cdef( [[
typedef struct {
	int x, y;
	unsigned int w, h;
} ASSI_Rect;

uint32_t    assi_getVersion( void );
const char* assi_getErrorString( void* );
void*       assi_init( int, int, const char*, const char* );
int         assi_setHeader( void*, const char* );
int         assi_setScript( void*, const char* );
int         assi_calculateBounds( void*, ASSI_Rect*, const int32_t*, const uint32_t );
void        assi_cleanup( void* );
]] )

-- Figure out a better way to load this?
extension = ('OSX' == ffi.os and '.dylib' or 'Windows' == ffi.os and '.dll' or '.so')
libraryPath = "?user/automation/include/ASSInspector"
success, ASSInspector = pcall( ffi.load, aegisub.decode_path( libraryPath .. "/libASSInspector" .. extension ) )
if not success
	libraryPath = "?data/automation/include/ASSInspector"
	success, ASSInspector = pcall( ffi.load, aegisub.decode_path( libraryPath .. "/libASSInspector" .. extension ) )
	if not success
		error( "Could not load required libASSInspector library." )


-- Inspector is not actually a property of the class but a property of
-- the script. Since we don't need to worry about concurrency or
-- anything silly like that, we can initialize ASSInspector once and
-- keep that reference alive until Aegisub is closed or scripts are all
-- reloaded.
state = {
	inspector: nil
	fontDir:   nil
	resX:      nil
	resY:      nil
}

collectHeader = ( subtitles ) =>
	scriptHeader = {
		"[Script Info]"
	}

	-- These are the only header fields that actually affect the way ASS
	-- is rendered. I don't actually know if ScriptType matters.
	infoFields = {
		PlayResX:   true
		PlayResY:   true
		WrapStyle:  true
		ScriptType: true
		ScaledBorderAndShadow: true
	}

	styles = { }

	seenStyles = false
	resX = 640
	resY = 480

	-- This loop assumes the subtitle script is nicely organized in the
	-- order [Script Info] -> [V4+ Styles] -> [Events]. This isn't
	-- guaranteed for scripts floating around in the wild, but fortunately
	-- Aegisub 3.2 seems to do a very good job of ensuring this order when
	-- a script is loaded. In other words, this loop is probably only safe
	-- to do from within automation.
	for index = 1, #subtitles
		with line = subtitles[index]
			if "info" == .class
				if infoFields[.key]
					table.insert( scriptHeader, .raw )

				if "PlayResX" == .key
					resX = tonumber( .value )
				elseif "PlayResY" == .key
					resY = tonumber( .value )

			elseif "style" == .class
				unless seenStyles
					table.insert( scriptHeader, "[V4+ Styles]\n" )
					seenStyles = true

				styles[.name] = .raw

			elseif "dialogue" == .class
				break

	-- If a video is loaded, use its resolution instead of the script's
	-- resolution. Don't use low-resolution workraws for typesetting!
	vidResX, vidResY = aegisub.video_size( )
	@resX = vidResX or resX
	@resY = vidResY or resY

	@header = table.concat( scriptHeader, '\n' )
	@styles = styles

initializeInspector = ( fontDirectory = state.fontDir ) =>
	if nil == state.inspector
		libVersion = ASSInspector.assi_getVersion!
		if libVersion < script_version
			return nil, "ASSInspector C library is out of date."
		elseif libVersion > script_version
			return nil, "ASSInspector moonscript library is out of date."

		state.inspector = ffi.gc( ASSInspector.assi_init( @resX, @resY, aegisub.decode_path( libraryPath .. "/fonts.conf" ), fontDirectory ), ASSInspector.assi_cleanup )

		if nil == state.inspector
			return nil, "ASSInspector library initialization failed."

		state.resX = @resX
		state.resY = @resY
		state.fontDir = fontDirectory

	return true

validateRect = ( rect ) ->
	bounds = {
		x: tonumber( rect.x )
		y: tonumber( rect.y )
		w: tonumber( rect.w )
		h: tonumber( rect.h )
	}

	if bounds.w == 0 or bounds.h == 0
		return false
	else
		return bounds

defaultTimes = ( lines ) ->
	ffms = aegisub.frame_from_ms
	msff = aegisub.ms_from_frame

	seenTimes = { }
	times = { }
	hasFrames = ffms( 0 )

	for line in *lines
		with line
			if (true == .assi_exhaustive) and hasFrames
				for frame = ffms( .start_time ), ffms( .end_time ) - 1
					frameTime = math.floor( 0.5*( msff( frame ) + msff( frame + 1 ) ) )
					unless seenTimes[frameTime]
						table.insert( times, frameTime )
						seenTimes[frameTime] = true
			else
				unless seenTimes[.start_time]
					table.insert( times, frameTime )
					seenTimes[.start_time] = true

	-- This will only happen if all lines are displayed for 0 frames.
	unless 0 < #times
		return false
	else
		return times

addStyles = ( line, scriptText, seenStyles ) =>
	if @styles[line.style] and not seenStyles[line.style]
		table.insert( scriptText, @styles[line.style] )
		seenStyles[line.style] = true

	line.text\gsub "{(.-)}", ( tagBlock ) ->
		tagBlock\gsub "\\r([^\\}]*)", ( styleName ) ->
			if 0 < #styleName and @styles[styleName] and not seenStyles[styleName]
				table.insert( scriptText, @styles[styleName] )
				seenStyles[styleName] = true

class Inspector

	new: ( subtitles, fontDirectory = aegisub.decode_path( '?script/fonts' ) ) =>
		if '?script/fonts' == fontDirectory
			fontDirectory = nil

		collectHeader( @, subtitles )
		success, message = initializeInspector( @, fontDirectory )
		if nil == success
			return success, message

		@updateHeader!

	updateHeader: ( subtitles ) =>
		if nil != subtitles
			collectHeader( @, subtitles )

		if @resX != state.resX or @resY != state.resY
			@forceLibraryReload!

		if 0 < ASSInspector.assi_setHeader( state.inspector, @header )
			return nil, "Failed to set header.\n" .. ffi.string( ASSInspector.assi_getErrorString( state.inspector ) )

	forceLibraryReload: ( fontDirectory ) =>
		state.inspector = nil
		initializeInspector( @, fontDirectory )

	-- Arguments:
	-- subtitles: the subtitles object that Aegisub passes to the macro.

	-- line: an array-like table of line tables that have at least the
	--       fields `start_time`, `text`, `end_time` and `raw`. `raw` is
	--       mandatory in all cases. `start_time` and `end_time` are only
	--       necessary if a table of times to render the line is not
	--       provided and a video is loaded. Allows rendering 1 or more
	--       lines simultaneously.

	-- times: a table of times (in milliseconds) to render the line at.
	--        Defaults to the start time of the line unless there is a video
	--        loaded and the line contains the field line.assi_exhaustive =
	--        true, in which case it defaults to every single frame that the
	--        line is displayed.

	-- Returns:
	-- An array-like table of bounding boxes, and an array-like table of
	-- render times, the two tables are the same length. If multiple lines
	-- are passed, the bounding box for a given time is the combined
	-- bounding boxes of all the lines.

	-- Error Handling:
	-- If an error is encounted, getBounds returns nil and an error string,
	-- which is typical for lua error reporting. In order to distinguish
	-- between an error and a valid false return, users should make sure
	-- they actually compare result to nil and false. Rather than just
	-- checking that the result is not falsy.

	getBounds: ( lines, times = defaultTimes( lines ) ) =>
		unless times
			return false

		scriptText = { }
		seenStyles = { }
		if @styles.Default
			seenStyles.Default = true
			table.insert( scriptText, @styles.Default )

		for line in *lines
			addStyles( @, line, scriptText, seenStyles )

		table.insert( scriptText, '[Events]' )

		for line in *lines
			table.insert( scriptText, line.raw )

		if 0 < ASSInspector.assi_setScript( state.inspector, table.concat( scriptText, '\n' ) )
			return nil, "Could not set script" .. ffi.string( ASSInspector.assi_getErrorString( state.inspector ) )

		renderCount = #times
		cTimes = ffi.new( 'int32_t[?]', renderCount )
		cRects = ffi.new( 'ASSI_Rect[?]', renderCount )

		for i = 0, renderCount - 1
			cTimes[i] = times[i + 1]

		if 0 < ASSInspector.assi_calculateBounds( state.inspector, cRects, cTimes, renderCount )
			return nil, "Error calculating bounds" .. ffi.string( ASSInspector.assi_getErrorString( state.inspector ) )

		rects = { }
		for i = 0, renderCount - 1
			table.insert( rects, validateRect( cRects[i] ) )

		return rects, times

return Inspector
