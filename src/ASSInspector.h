/* ASSInspector is free software. You can redistribute it and/or modify
 * it under the terms of the MIT License. See COPYING or do a google
 * search for details. */

#pragma once

#include <stdint.h>

#ifdef _WIN32
#	ifdef ASS_INSPECTOR_STATIC
#		define ASSI_EXPORT
#	else
#		ifdef BUILD_DLL
#			define ASSI_EXPORT __declspec(dllexport)
#		else
#			define ASSI_EXPORT __declspec(dllimport)
#		endif
#	endif // ASS_INSPECTOR_STATIC
#else // Non-windows
#	define ASSI_EXPORT
#endif // _WIN32

#define ASSI_VERSION 0x000003

typedef struct ASSI_State_priv ASSI_State;

ASSI_EXPORT uint32_t    assi_getVersion( void );
ASSI_EXPORT ASSI_State* assi_init( int, int );
ASSI_EXPORT void        assi_addHeader( ASSI_State*, const char*, unsigned int );
ASSI_EXPORT int         assi_initEvents( ASSI_State*, unsigned int count );
ASSI_EXPORT int         assi_addEvent( ASSI_State*, const char*, unsigned int, unsigned int );
ASSI_EXPORT int         assi_checkLine( ASSI_State*, const int, const int*, const unsigned int, uint8_t* );
ASSI_EXPORT void        assi_cleanup( ASSI_State* );
