/* SubInspector is free software. You can redistribute it and/or modify
 * it under the terms of the MIT License. See COPYING or do a google
 * search for details. */

#pragma once

#include <stdint.h>
#include <string.h> // size_t

#ifdef _WIN32
	#ifdef SUB_INSPECTOR_STATIC
		#define SI_EXPORT
	#else
		#ifdef BUILD_DLL
			#define SI_EXPORT __declspec(dllexport)
		#else
			#define SI_EXPORT __declspec(dllimport)
		#endif // BUILD_DLL
	#endif // SUB_INSPECTOR_STATIC
#else // Non-windows
	#define SI_EXPORT
#endif // _WIN32

#define SI_VERSION 0x000502

typedef struct SI_State_priv SI_State;

typedef struct {
	int x, y;
	unsigned int w, h;
	uint32_t hash;
	uint8_t solid;
} SI_Rect;

SI_EXPORT uint32_t    si_getVersion( void );
SI_EXPORT const char* si_getErrorString( SI_State *state );
SI_EXPORT SI_State*   si_init( int width, int height, const char* fontConfigConfig, const char *fontDir );
SI_EXPORT void        si_changeResolution( SI_State *state, int width, int height );
SI_EXPORT void        si_reloadFonts( SI_State *state, const char *fontConfigConfig, const char *fontDir );
SI_EXPORT int         si_setHeader( SI_State *state, const char *header, size_t length );
SI_EXPORT int         si_setScript( SI_State *state, const char *body, size_t length );
SI_EXPORT int         si_calculateBounds( SI_State *state, SI_Rect *rects, const int32_t *times, const uint32_t renderCount );
SI_EXPORT void        si_cleanup( SI_State *state );
