-- This library is unlicensed under CC0

DependencyControl = require "l0.DependencyControl"

versionRecord = DependencyControl( {
	name: "SubInspector",
	version: "0.6.1",
	description: "Provides low level inspection and analysis of subtitles post-rasterization.",
	author: "torque",
	url: "https://github.com/TypesettingCartel/SubInspector",
	moduleName: "SubInspector.Inspector",
	feed: "https://raw.githubusercontent.com/TypesettingCartel/SubInspector/master/DependencyControl.json",
	{ "ffi" }
} )

SIVersionCompat = DependencyControl( {
	moduleName: "SubInspector.Compat",
	version: "0.5.1",
	virtual: true
} )

ffi = versionRecord\requireModules!

ffi.cdef( [[
typedef struct {
	int x, y;
	unsigned int w, h;
	uint32_t hash;
	uint8_t solid;
} SI_Rect;

uint32_t    si_getVersion( void );
const char* si_getErrorString( void* );
void*       si_init( int, int, const char*, const char* );
void        si_changeResolution( void*, int, int );
void        si_reloadFonts( void*, const char*, const char* );
int         si_setHeader( void*, const char*, size_t );
int         si_setScript( void*, const char*, size_t );
int         si_calculateBounds( void*, SI_Rect*, const int32_t*, const uint32_t );
void        si_cleanup( void* );
]] )

packagePaths = ( namespace ) ->
	paths = { }
	-- fixedLibraryName = namespace .. "/" .. "#{(ffi.os != 'Windows') and 'lib' or ''}#{libraryName}.#{(OSX: 'dylib', Windows: 'dll')[ffi.os] or 'so'}"
	package.path\gsub "([^;]+)", ( path ) ->
		-- the init.lua paths are just dupes of other paths.
		if path\match "/%?/init%.lua$"
			return

		path = path\gsub "//?%?%.lua$", "/"
		table.insert paths, path .. namespace

	return paths

__name = "SubInspector"
libraryPaths = packagePaths "#{__name}/Inspector/"
libraryName = "#{(ffi.os != 'Windows') and 'lib' or ''}#{__name}.#{(OSX: 'dylib', Windows: 'dll')[ffi.os] or 'so'}"
local SubInspector, libraryPath
for path in *libraryPaths
	success, SubInspector = pcall ffi.load, path .. libraryName
	if success
		libraryPath = path
		break

assert libraryPath, "Could not load #{__name} C library."

-- Returns true if the versions are compatible and nil, "message" if
-- they aren't.
looseVersionCompare = ( ASSIVersion ) ->
	siVer = DependencyControl moduleName: "SubInspector.Lib", version: ASSIVersion, virtual: true
	unless SIVersionCompat\checkVersion siVer, "major"
		return nil, ("Inspector.moon library is too old. Must be v%s.")\format siVer\getVersionString nil, "major"
	unless siVer\checkVersion SIVersionCompat, "minor"
		return nil, ("libSubInspector library is too old. Must be v%s compatible.")\format SIVersionCompat\getVersionString nil, "minor"

	return true

-- Inspector is not actually a property of the class but a property of
-- the script. Since we don't need to worry about concurrency or
-- anything silly like that, we can initialize SubInspector once and
-- keep that reference alive until Aegisub is closed or scripts are all
-- reloaded.
state = {
	inspector:        nil
	fontconfigConfig: nil
	fontDir:          nil
	resX:             nil
	resY:             nil
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

initializeInspector = ( fontconfigConfig = state.fontconfigConfig, fontDirectory = state.fontDir ) ->
	if nil == state.inspector
		success, message = looseVersionCompare( SubInspector.si_getVersion! )
		unless success
			return nil, message

		state.inspector = ffi.gc( SubInspector.si_init( 1, 1, fontconfigConfig, fontDirectory ), SubInspector.si_cleanup )

		if nil == state.inspector
			return nil, "SubInspector library initialization failed."

		state.resX = 1
		state.resY = 1
		state.fontDir = fontDirectory
		state.fontconfigConfig = fontconfigConfig

	return true

validateRect = ( rect ) ->
	bounds = {
		x: tonumber( rect.x )
		y: tonumber( rect.y )
		w: tonumber( rect.w )
		h: tonumber( rect.h )
		hash: tonumber( rect.hash )
		solid: (rect.solid == 1)
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

	if hasFrames
		for line in *lines
			with line
				for frame = ffms( .start_time ), true == .si_exhaustive and ffms( .end_time ) - 1 or ffms( .start_time )
					frameTime = math.floor( 0.5*( msff( frame ) + msff( frame + 1 ) ) )
					unless seenTimes[frameTime]
						table.insert( times, frameTime )
						seenTimes[frameTime] = true
	else
		for line in *lines
			with line
				unless seenTimes[.start_time]
					table.insert( times, .start_time )
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
	@version = versionRecord

	new: ( subtitles, fontconfigConfig = libraryPath .. "fonts.conf", fontDirectory = aegisub.decode_path( '?script/fonts' ) ) =>
		assert subtitles, "You must provide the subtitles object."

		-- Does nothing if inspector is already initialized. The initialized
		-- SubInspector is stored at the script scope, which means it
		-- persists until either Aegisub quits, automation scripts are
		-- reloaded, or forceLibraryReload is called.
		success, message = initializeInspector( fontconfigConfig, fontDirectory )
		assert success, message

		success, message = @updateHeader( subtitles )
		assert success, message

	updateHeader: ( subtitles ) =>
		if nil == subtitles
			return nil, "You must provide the subtitles object."

		collectHeader( @, subtitles )

		if @resX != state.resX or @resY != state.resY
			state.resX, state.resY = @resX, @resY
			SubInspector.si_changeResolution( state.inspector, @resX, @resY )

		@reloadFonts!

		if 0 < SubInspector.si_setHeader( state.inspector, @header, #@header )
			return nil, "Failed to set header.\n" .. ffi.string( SubInspector.si_getErrorString( state.inspector ) )

		return true

	reloadFonts: ( fontconfigConfig = state.fontconfigConfig, fontDirectory = state.fontDir ) =>
		SubInspector.si_reloadFonts( state.inspector, fontconfigConfig, fontDirectory )

	forceLibraryReload: ( subtitles, fontconfigConfig, fontDirectory ) =>
		if nil == subtitles
			return nil, "You must provide the subtitles object."

		state.inspector = nil
		-- let initializeInspector handle the default arguments.
		success, message = initializeInspector( fontconfigConfig, fontDirectory )
		if nil == success
			return success, message

		@updateHeader( subtitles )

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
	--        loaded and the line contains the field line.si_exhaustive =
	--        true, in which case it defaults to every single frame that the
	--        line is displayed.

	-- Returns:
	-- An array-like table of bounding boxes, and an array-like table of
	-- render times. The two tables are the same length. If multiple lines
	-- are passed, the bounding box for a given time is the combined
	-- bounding boxes of all the lines rendered at that time.

	-- Error Handling:
	-- If an error is encountered, getBounds returns nil and an error string,
	-- which is typical for lua error reporting. In order to distinguish
	-- between an error and a valid false return, users should make sure
	-- they actually compare result to nil and false, rather than just
	-- checking that the result is not falsy.

	getBounds: ( lines, times = defaultTimes( lines ) ) =>
		unless times
			return nil, "The render times table was empty."

		for i = 1,#lines
			line = lines[i]
			if nil == line.raw
				if line.createRaw
					line\createRaw!
				else
					return nil, "line.raw is missing from line #{i}."

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

		scriptString = table.concat( scriptText, '\n' )
		if 0 < SubInspector.si_setScript( state.inspector, scriptString, #scriptString )
			return nil, "Could not set script" .. ffi.string( SubInspector.si_getErrorString( state.inspector ) )

		renderCount = #times
		cTimes = ffi.new( 'int32_t[?]', renderCount )
		cRects = ffi.new( 'SI_Rect[?]', renderCount )

		for i = 0, renderCount - 1
			cTimes[i] = times[i + 1]

		if 0 < SubInspector.si_calculateBounds( state.inspector, cRects, cTimes, renderCount )
			return nil, "Error calculating bounds" .. ffi.string( SubInspector.si_getErrorString( state.inspector ) )

		rects = { }
		for i = 0, renderCount - 1
			table.insert( rects, validateRect( cRects[i] ) )

		return rects, times

return versionRecord\register Inspector
