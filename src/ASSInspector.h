/* ASSInspector is free software. You can redistribute it and/or modify
 * it under the terms of the MIT License. See COPYING or do a google
 * search for details. */

#pragma once

#include <stdint.h>
#include <ass/ass.h>

typedef struct {
	ASS_Library   *assLibrary;
	ASS_Renderer  *assRenderer;
	char          *header;
	char         **events;
	unsigned int  *eventLengths, headerLength, eventCount;
} ASSI_State;


uint32_t    assi_getVersion( void );
ASSI_State* assi_init( int, int );
void        assi_addHeader( ASSI_State*, const char*, unsigned int );
void        assi_initEvents( ASSI_State*, unsigned int count );
void        assi_addEvent( ASSI_State*, const char*, unsigned int, unsigned int );
int         assi_checkLine( ASSI_State*, const int, const int*, const unsigned int, uint8_t* );
void        assi_cleanup( ASSI_State* );
