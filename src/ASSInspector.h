/* ASSInspector is free software. You can redistribute it and/or modify
 * it under the terms of the MIT License. See COPYING or do a google
 * search for details. */

#pragma once

#include <stdint.h>

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

#define ASSI_VERSION 0x000003

typedef struct ASSI_State_priv ASSI_State;

typedef struct {
	int32_t x, y;
	uint32_t w, h;
} ASSI_Rect;

ASSI_EXPORT uint32_t    assi_getVersion( void );
ASSI_EXPORT ASSI_State* assi_init( int, int, const char*, uint32_t );
ASSI_EXPORT int         assi_setScript( ASSI_State*, const char*, uint32_t, const char *, uint32_t );
ASSI_EXPORT int         assi_calculateBounds( ASSI_State*, ASSI_Rect**, int32_t*, uint32_t );
ASSI_EXPORT void        assi_cleanup( ASSI_State* );
