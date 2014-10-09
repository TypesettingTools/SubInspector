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


uint32_t    ASSI_getVersion( void );
ASSI_State* ASSI_init( int, int );
void        ASSI_addHeader( ASSI_State*, const char*, unsigned int );
void        ASSI_initEvents( ASSI_State*, unsigned int count );
void        ASSI_addEvent( ASSI_State*, const char*, unsigned int, unsigned int );
int         ASSI_checkLine( ASSI_State*, const int, const int*, const unsigned int, uint8_t* );
void        ASSI_cleanup( ASSI_State* );
