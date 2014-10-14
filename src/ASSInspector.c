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
	char         *header,
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

	char *tempHeader = calloc( headerLength, sizeof(*tempHeader) );
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
		free( state->currentScript );
		ass_renderer_done( state->assRenderer );
		ass_library_done( state->assLibrary );
		free( state );
	}
}

int assi_setScript( ASSI_State *state, const char *styles, uint32_t stylesLength, const char *events, const uint32_t eventsLength ) {
	if ( !state ) {
		return 1;
	}
	if ( state->currentScript ) {
		free( state->currentScript );
		state->currentScript = NULL;
	}

	uint32_t tempScriptLength = state->headerLength + stylesLength + eventsLength;
	char *tempScript = malloc( tempScriptLength * sizeof(*tempScript) );
	if ( NULL == tempScript ) {
		strcpy( state->error, "Memory allocation failure." );
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

	return 0;
}

int assi_calculateBounds( ASSI_State *state, ASSI_Rect *rects, const int32_t *times, const uint32_t renderCount ) {
	if ( !state ) {
		return 1;
	}

	ASS_Track *assTrack = ass_read_memory( state->assLibrary, state->currentScript, state->scriptLength, NULL );
	if ( NULL == assTrack ) {
		return 1;
	}

	for ( int i = 0; i < renderCount; ++i ) {
		ASS_Image *assImage = ass_render_frame( state->assRenderer, assTrack, times[i], NULL );
		// ASS_Image is apparently a linked list.
		while ( assImage ) {
			assImage = assImage->next;
		}
	}

	ass_free_track( assTrack );
	return 0;
}

