/* ASSInspector is free software. You can redistribute it and/or modify
 * it under the terms of the MIT license. See COPYING or do a google
 * search for details. */

#include "ASSInspector.h"

#include <stdlib.h>
#include <string.h>
#include <ass/ass.h>

struct ASSI_State_priv{
	ASS_Library  *assLibrary;
	ASS_Renderer *assRenderer;
	uint8_t      *header,
	             *currentScript;
	uint32_t      headerLength,
	              scriptLength;
	char error[128];
};

static void msgCallback( int, const char*, va_list, void* );

static void msgCallback( int level, const char *fmt, va_list va, void *data ) {
	if ( level < 4 ) {
		ASSI_State *state = data;
		int levelLength = sprintf( state->error, "%d: ", level );
		vsnprintf( state->error + levelLength, sizeof(state->error) - levelLength, fmt, va );
	}
}

uint32_t assi_getVersion( void ) {
	return ASSI_VERSION;
}

ASSI_State* assi_init( int width, int height, const char *header, uint32_t headerLength ) {
	ASSI_State *state = calloc( 1, sizeof(*state) );
	if ( NULL == state ) {
		return NULL;
	}

	state->assLibrary = ass_library_init( );
	if ( NULL == state->assLibrary) {
		free( state );
		return NULL;
	}
	ass_set_message_cb( state->assLibrary, msgCallback, state );

	state->assRenderer = ass_renderer_init( state->assLibrary );
	if ( NULL == state->assRenderer ) {
		ass_library_done( state->assLibrary );
		free( state );
		return NULL;
	}

	ass_set_frame_size( state->assRenderer, width, height );
	ass_set_fonts( state->assRenderer, NULL, "Sans", 1, NULL, 1 );

	uint8_t *tempHeader = calloc( headerLength, sizeof(*tempHeader) );
	if ( NULL == tempHeader ) {
		ass_renderer_done( state->assRenderer );
		ass_library_done( state->assLibrary );
		free( state );
		return NULL;
	}
	memcpy( tempHeader, header, headerLength );
	state->header = tempHeader;
	state->headerLength = headerLength;

	return state;
}

void assi_cleanup( ASSI_State *state ) {
	if ( state ) {
		free( state->header );
		ass_renderer_done( state->assRenderer );
		ass_library_done( state->assLibrary );
		free( state );
	}
}

int assi_setScript( ASSI_State *state, const char *styles, uint32_t stylesLength, const char *events, uint32_t eventsLength ) {
	if ( !state ) {
		return 1;
	}
	if ( state->currentScript ) {
		free( state->currentScript );
	}

	uint32_t tempScriptLength = state->headerLength + stylesLength + eventsLength;
	uint8_t *tempScript = malloc( tempScriptLength * sizeof(*tempScript) );
	if ( NULL == tempScript ) {
		state->error = "Memory allocation failure.";
		return 1;
	}
	// Copy the header.
	memcpy( tempScript, state->header, state->headerLength );
	// If styles are provided, copy them.
	if ( NULL != styles && stylesLength > 0 ) {
		memcpy( tempScript + state->headerLength, styles, stylesLength );
	}
	// Copy the events.
	memcpy( tempScript + state->headerLength + stylesLength, events, eventsLength );

	state->currentScript = tempScript;
	state->scriptLength = tempScriptLength;
}
