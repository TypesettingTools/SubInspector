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
	char          *header;
	char         **events;
	unsigned int  *eventLengths, headerLength, eventCount;
};

static uint8_t findDirty( ASS_Image* );
static uint8_t processFrame( ASS_Renderer*, ASS_Track*, int );
static void msgCallback( int, const char*, va_list, void* );

static void msgCallback( int level, const char *fmt, va_list va, void *data ) {
	return;
}

uint32_t assi_getVersion( void ) {
	return ASSI_VERSION;
}

ASSI_State* assi_init( int width, int height ) {
	ASSI_State *state = malloc( sizeof *state );
	if ( NULL == state ) {
		return NULL;
	}

	state->assLibrary = ass_library_init( );
	if ( NULL == state->assLibrary) {
		free( state );
		return NULL;
	}
	ass_set_message_cb( state->assLibrary, msgCallback, NULL );

	state->assRenderer = ass_renderer_init( state->assLibrary );
	if ( NULL == state->assRenderer ) {
		ass_library_done( state->assLibrary );
		free( state );
		return NULL;
	}

	ass_set_frame_size( state->assRenderer, width, height );
	ass_set_fonts( state->assRenderer, NULL, "Sans", 1, NULL, 1 );

	state->events = NULL;
	state->eventLengths = NULL;
	state->eventCount = 0;

	return state;
}

void assi_addHeader( ASSI_State *state, const char *newHeader, unsigned int length ) {
	// should probably be copied, too
	state->header = (char *)newHeader;
	state->headerLength = length;
}

int assi_initEvents( ASSI_State *state, unsigned int count ) {
	state->events = malloc( count * sizeof *state->events );
	if( NULL == state->events ) {
		return 1;
	}
	memset( state->events, 0, count * sizeof *state->events );
	state->eventLengths = malloc( count * sizeof *state->eventLengths );
	if( NULL == state->eventLengths ) {
		free( state->events );
		state->events = NULL;
		return 1;
	}
	state->eventCount = count;
	return 0;
}

int assi_addEvent( ASSI_State *state, const char *event, unsigned int length, unsigned int index ) {
	char *tempEvent = malloc( length * sizeof *tempEvent );
	if( NULL == tempEvent ) {
		return 1;
	}
	memcpy( tempEvent, event, length );
	free( state->events[index] );
	state->events[index] = tempEvent;
	state->eventLengths[index] = length;
	return 0;
}

void assi_cleanup( ASSI_State *state ) {
	for ( unsigned int index = 0; index < state->eventCount; ++index ) {
		free( state->events[index] );
	}
	free( state->events );
	free( state->eventLengths );
	ass_renderer_done( state->assRenderer );
	ass_library_done( state->assLibrary );
	free( state );
}

// Takes the index (0-based) of the event to render and an array of times
// (corresponding to the frames of the event). Modifies the array
// `result` that is passed in. Returns an error if libass fails to read
// the event.
int assi_checkLine( ASSI_State *state, const int eventIndex, const int *times, const unsigned int timesLength, uint8_t *result ) {
	// Merge the header and the desired event. None of this needs to be
	// null terminated because we have the length of everything.
	int scriptLength = state->headerLength + state->eventLengths[eventIndex];
	char *script = malloc( scriptLength * sizeof *script );
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
