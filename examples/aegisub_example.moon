-- This example is unlicensed under CC0

export script_name        = "ASSInspector Example"
export script_description = "Calculates bounding rectangles on the start and end times of selected events."
export script_author      = "torque"
export script_version     = 0x000003

ffi  = require( 'ffi' )
util = require( 'aegisub.util' )
log  = require( 'a-mo.log' )

ffi.cdef( [[
typedef struct {
	int32_t x, y;
	uint32_t w, h;
} ASSI_Rect;

uint32_t    assi_getVersion( void );
const char* assi_getErrorString( void* );
void*       assi_init( int, int, const char*, uint32_t, const char*, const char* );
int         assi_setScript( void*, const char*, uint32_t, const char *, const uint32_t );
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

-- Script-wide local variables are stored until the script is reloaded,
-- at which point they are garbage collected. We can abuse this system
-- to create a persistent ASSInspector state that only gets deallocated
-- when the automation script is reloaded or Aegisub is quit.

-- NOTE: this can be somewhat risky. For example, loading a new subtitle
-- file in Aegisub won't reset the automation scripts, and thus the old
-- header information will continue being used. It would be better to
-- make some uniqueness check on the loaded script. The full path to the
-- script would be an option, except that doesn't work for scripts that
-- have yet to be saved to disk. Appending some sort of UUID to the
-- script header and checking that each run would probably be more
-- foolproof. This implementation detail is left as an exercise to the
-- user.
inspector = nil
initializeInspector = ( resX, resY, minimalHeader ) ->
	if nil == inspector
		if '?script' != aegisub.decode_path( '?script' )
			-- Tell ASSInspector to tell fontconfig to search the fonts
			-- directory in the script directory for fonts to load.
			inspector = ffi.gc( ASSInspector.assi_init( resX, resY, minimalHeader, #minimalHeader, aegisub.decode_path( libraryPath .. "/fonts.conf" ), aegisub.decode_path( '?script/fonts' ) ), ASSInspector.assi_cleanup )
		else
			inspector = ffi.gc( ASSInspector.assi_init( resX, resY, minimalHeader, #minimalHeader, aegisub.decode_path( libraryPath .. "/fonts.conf" ), nil ), ASSInspector.assi_cleanup )
		if nil == inspector
			log.windowError( "ASSInspector library init failed." )

dumpRect = ( event, rect ) ->
	bounds = {
		x:  tonumber( rect.x )
		y:  tonumber( rect.y )
		x2: tonumber( rect.w + rect.x )
		y2: tonumber( rect.h + rect.y )
	}

	newEvent = util.deep_copy( event )
	with bounds
		newEvent.text = ([[{\an7\pos(0,0)\bord0\shad0\c&H0000FF&\1a&H00&\p1}m %d %d l %d %d %d %d %d %d]])\format( .x, .y, .x2, .y, .x2, .y2, .x, .y2 )

	event.layer += 1
	return newEvent

mainFunction = ( subtitle, selectedLines, activeLine ) ->
	if ASSInspector.assi_getVersion( ) < script_version
		log.windowError( "Installed ASSInspector library is outdated. Please update it." )
		return selectedLines
	elseif ASSInspector.assi_getVersion( ) > script_version
		log.windowError( "Installed OptimASS is outdated. Please update it." )
		return selectedLines


	scriptHeader = {
		"[Script Info]"
	}

	-- These are the only header fields that actually affect the way ASS
	-- is rendered. I don't actually know if ScriptType makes a matters.
	infoFields = {
		PlayResX:   true
		PlayResY:   true
		WrapStyle:  true
		ScriptType: true
		ScaledBorderAndShadow: true
	}

	styles = { }

	-- If there is no video (or, at minimum, timecodes) loaded, this will
	-- return nil.
	hasTimecodes = aegisub.frame_from_ms( 0 )

	-- Copy these functions into local variables to shorten them.
	ffms = aegisub.frame_from_ms
	msff = aegisub.ms_from_frame

	subtitleLen = #subtitle
	seenStyles = false
	resX = 640
	resY = 480

	-- This loop assumes the subtitle script is nicely organized in the
	-- order [Script Info] -> [V4+ Styles] -> [Events]. This isn't
	-- guaranteed for scripts floating around in the wild, but fortunately
	-- Aegisub 3.2 seems to do a very good job of ensuring this order when
	-- a script is loaded. In other words, this loop is probably only safe
	-- to do from within automation.
	for index = 1, subtitleLen
		with line = subtitle[index]
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
	-- resolution. Not convinced this is a great idea because people may
	-- use smaller workraws or something like that. Well, hopefully they
	-- don't do that for typesetting.
	vidResX, vidResY = aegisub.video_size( )
	resX = vidResX or resX
	resY = vidResY or resY

	minimalHeader = table.concat( scriptHeader, '\n' )

	initializeInspector( resX, resY, minimalHeader )

	insertIndices = { }
	insertEvents  = { }

	for eventIndex in *selectedLines
		with event = subtitle[eventIndex]
			-- This does not account for \r[name] being in the line.
			styleText = (styles[.style] or "") .. "\n[Events]\n"
			eventText = .raw

			-- It occurs to me that perhaps having the user just pass in one
			-- string that contains both styles and events may be easier.
			if 0 < ASSInspector.assi_setScript( inspector, styleText, #styleText, eventText, #eventText )
				log.warn( ASSInspector.assi_getErrorString( inspector ) )
				ASSInspector.assi_cleanup( inspector )
				aegisub.cancel( )

			-- We're going to render a line twice, once at its start and once
			-- at its end.
			renderCount = 2

			renderTimes = { }

			-- Cheat a bit when generating the times to render. This obviously
			-- will not scale with a renderCount > 2. Use 0-index arrays to
			-- match up with C indices in a later loop.
			if hasTimecodes
				-- Using the aegisub.frame_from_ms function pointers we copied
				-- earlier.
				startFrame = ffms( .start_time )
				endFrame   = ffms( .end_time ) - 1
				renderTimes[0] = math.floor( 0.5*( msff( startFrame ) + msff( startFrame + 1 ) ) )
				renderTimes[1] = math.floor( 0.5*( msff( endFrame ) + msff( endFrame + 1 ) ) )
			else
				renderTimes[0] = .start_time
				-- Trying to render exactly at end_time will result in a
				-- completely blank frame.
				renderTimes[1] = .end_time - 1

			-- Because we're rendering the line twice, we need two rects.
			rects = ffi.new( "ASSI_Rect[?]", renderCount )

			-- Allocate the array of times for rendering.
			times = ffi.new( "int32_t[?]", renderCount )

			-- Set up the render times. There's no real reason to do this in a
			-- loop if you know beforehand you're only going to render the
			-- line twice, but loops are more generic.
			for render = 0, renderCount - 1
				times[render] = renderTimes[render]

			-- Send the data in for rendering and bounds calculation.
			if 0 < ASSInspector.assi_calculateBounds( inspector, rects, times, renderCount )
				log.warn( ASSInspector.assi_getErrorString( inspector ) )
				aegisub.cancel( )

			-- Check out the calculated bounding rects
			for render = 0, renderCount - 1
				if rects[render].w > 0 and rects[render].h > 0
					newEvent = dumpRect( event, rects[render] )
					table.insert( insertIndices, eventIndex )
					table.insert( insertEvents, newEvent)

			subtitle[eventIndex] = event

	for index = #insertIndices, 1, -1
		subtitle.insert( insertIndices[index], insertEvents[index] )


aegisub.register_macro( script_name, script_description, mainFunction )
