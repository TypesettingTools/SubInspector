/* ASSInspector is free software. You can redistribute it and/or modify
 * it under the terms of the MIT license. See COPYING or do a google
 * search for details. */

#include "ASSInspector.h"

#include <stdlib.h>
#include <string.h>
#include <ass/ass.h>

struct ASSI_State_priv{
	ASS_Library   *assLibrary;
	ASS_Renderer  *assRenderer;
	uint8_t       *header;
	uint32_t      headerLength;
	char error[128];
};

static uint8_t findDirty( ASS_Image* );
static uint8_t processFrame( ASS_Renderer*, ASS_Track*, int );
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

ASSI_State* assi_init( int width, int height ) {
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

	return state;
}

int assi_addHeader( ASSI_State *state, const char *header, unsigned int length ) {
	if( NULL == state ){
		return 1;
	}
	char *tempHeader = malloc( length );
	if( NULL == tempHeader ){
		return 1;
	}
	memcpy( tempHeader, header, length );
	free( state->header );
	state->header = tempHeader;
	state->headerLength = length;
	return 0;

}

void assi_cleanup( ASSI_State *state ) {
	if ( state ) {
		free( state->header );
		ass_renderer_done( state->assRenderer );
		ass_library_done( state->assLibrary );
		free( state );
	}
}

// Takes the index (0-based) of the event to render and an array of times
// (corresponding to the frames of the event). Modifies the array
// `result` that is passed in. Returns an error if libass fails to read
// the event.
int assi_checkLine( ASSI_State *state, const int eventIndex, const int *times, const unsigned int timesLength, uint8_t *result ) {
	if( NULL == state || NULL == state->header || NULL == state->events || NULL == state->events[eventIndex] ){
		return 1;
	}
	// Merge the header and the desired event. None of this needs to be
	// null terminated because we have the length of everything.
	int scriptLength = state->headerLength + state->eventLengths[eventIndex];
	char *script = malloc( scriptLength );
	if( NULL == script ) {
		return 1;
	}
	memcpy( script, state->header, state->headerLength );
	memcpy( script + state->headerLength, state->events[eventIndex], state->eventLengths[eventIndex] );

	ASS_Track *assTrack = ass_read_memory( state->assLibrary, script, scriptLength, NULL );
	if ( NULL == assTrack ) {
		free( script );
		return 1;
	}

	for ( int timeIdx = 0; timeIdx < timesLength; ++timeIdx ) {
		result[timesLength] |= result[timeIdx] = processFrame( state->assRenderer, assTrack, times[timeIdx] );
	}

	ass_free_track( assTrack );
	free( script );

	return 0;
}

static uint8_t findDirty( ASS_Image *img ) {
	// If alpha is not 255, the image is not fully transparent and we need
	// to check the bitmap blending mask to verify if it is dirty or not.
	if( 0xFF != (img->color & 0xFF) ) {
		uint8_t *bitmap = img->bitmap,
			*endOfRow;

		const uint8_t *endOfBitmap = bitmap + img->h * img->stride,
			       widthRemainder = img->w % sizeof( uintptr_t );

		const uint16_t padding = img->stride - img->w,
			       widthX = img->w / sizeof( uintptr_t );

		uintptr_t *bitmap_X,
			 *endOfRow_X;

		while ( bitmap < endOfBitmap ) {
			bitmap_X   = (uintptr_t *)bitmap;
			endOfRow_X = bitmap_X + widthX;
			while ( bitmap_X < endOfRow_X ) {
				if ( *bitmap_X++ ) {
					return 1;
				}
			}
			bitmap = (uint8_t *)bitmap_X;
			endOfRow = bitmap + widthRemainder;
			while ( bitmap < endOfRow ) {
				if ( *bitmap++ ) {
					return 1;
				}
			}
			bitmap += padding;
		}
	}
	return 0;
}

static uint8_t processFrame( ASS_Renderer *assRenderer, ASS_Track *assTrack, int frameTime ) {
	ASS_Image *assImage = ass_render_frame( assRenderer, assTrack, frameTime, NULL );

	// ASS_Image is apparently a linked list.
	while ( assImage ) {
		if ( findDirty( assImage ) ) {
			return 1;
		}
		assImage = assImage->next;
	}

	return 0;
}
