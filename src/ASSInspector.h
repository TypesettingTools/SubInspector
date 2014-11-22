/* ASSInspector is free software. You can redistribute it and/or modify
 * it under the terms of the MIT License. See COPYING or do a google
 * search for details. */

#pragma once

#include <stdint.h>
#include <string.h> // size_t

#ifdef _WIN32
	#ifdef ASS_INSPECTOR_STATIC
		#define ASSI_EXPORT
	#else
		#ifdef BUILD_DLL
			#define ASSI_EXPORT __declspec(dllexport)
		#else
			#define ASSI_EXPORT __declspec(dllimport)
		#endif // BUILD_DLL
	#endif // ASS_INSPECTOR_STATIC
#else // Non-windows
	#define ASSI_EXPORT
#endif // _WIN32

#define ASSI_VERSION 0x000101

typedef struct ASSI_State_priv ASSI_State;

typedef struct {
	int x, y;
	unsigned int w, h;
} ASSI_Rect;

ASSI_EXPORT uint32_t    assi_getVersion( void );
ASSI_EXPORT const char* assi_getErrorString( ASSI_State *state );
ASSI_EXPORT ASSI_State* assi_init( int width, int height, const char* fontConfigConfig, const char *fontDir );
ASSI_EXPORT int         assi_setHeader( ASSI_State *state, const char *header, size_t length );
ASSI_EXPORT int         assi_setScript( ASSI_State *state, const char *body, size_t length );
ASSI_EXPORT int         assi_calculateBounds( ASSI_State *state, ASSI_Rect *rects, const int32_t *times, const uint32_t renderCount );
ASSI_EXPORT void        assi_cleanup( ASSI_State *state );
