#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ass/ass.h>

#include "OptimASS.h"

static uint8_t findDirty( ASS_Image* );
static uint8_t processFrame( ASS_Track*, int );
static void msgCallback( int, const char*, va_list, void* );

static ASS_Library  *assLibrary;
static ASS_Renderer *assRenderer;
static char         *header;
static unsigned int  headerLength;
static char        **events;
static unsigned int *eventLengths;
static unsigned int  eventCount;

static void msgCallback( int level, const char *fmt, va_list va, void *data ) {
	return;
}

int optimASS_init( int width, int height ) {
	assLibrary = ass_library_init( );
	if ( NULL == assLibrary) {
		return 1;
	}
	ass_set_message_cb( assLibrary, msgCallback, NULL );

	assRenderer = ass_renderer_init( assLibrary );
	if ( NULL == assRenderer ) {
		ass_library_done( assLibrary );
		return 1;
	}

	ass_set_frame_size( assRenderer, width, height );
	// Set fallback fonts
	ass_set_fonts( assRenderer, NULL, "Sans", 1, NULL, 1 );
	return 0;
}

void optimASS_addHeader( const char *newHeader, unsigned int length ) {
	header = (char *)newHeader;
	headerLength = length;
}

void optimASS_initEvents( unsigned int count ) {
	events = malloc( count * sizeof *events );
	eventLengths = malloc( count * sizeof *eventLengths );
	eventCount = count;
}

void optimASS_addEvent( const char *event, unsigned int length, unsigned int index ) {
	char *tempEvent = malloc( length * sizeof *tempEvent );
	memcpy( tempEvent, event, length );
	events[index] = tempEvent;
	eventLengths[index] = length;
}

void optimASS_cleanup( void ) {
	for ( unsigned int index = 0; index < eventCount; ++index ) {
		free( events[index] );
	}
	free( events );
	free( eventLengths );
	ass_renderer_done( assRenderer );
	ass_library_done( assLibrary );
}

// Takes the index (0-based) of the event to render and an array of times
// (corresponding to the frames of the event). Modifies the array
// `result` that is passed in. Returns an error if libass fails to read
// the event.
int optimASS_checkLine( const int eventIndex, const int *times, const unsigned int timesLength, uint8_t *result ) {
	// Merge the header and the desired event. None of this needs to be
	// null terminated because we have the length of everything.
	int scriptLength = headerLength + eventLengths[eventIndex];
	char *script = malloc( scriptLength * sizeof *script );
	memcpy( script, header, headerLength );
	memcpy( script + headerLength, events[eventIndex], eventLengths[eventIndex] );

	ASS_Track *assTrack = ass_read_memory( assLibrary, script, scriptLength, NULL );
	if ( NULL == assTrack ) {
		free( script );
		return 1;
	}

	for ( int timeIdx = 0; timeIdx < timesLength; ++ timeIdx ) {
		result[timeIdx] = processFrame( assTrack, times[timeIdx] );
		result[timesLength] |= result[timeIdx];
	}

	ass_free_track( assTrack );
	free( script );

	return 0;
}

static uint8_t findDirty( ASS_Image *img ) {
	// If alpha is 255, the image is fully transparent and we can
	// determine it is not dirty immediately.
	if ( 0xFF == (img->color & 0xFF) ) {
		return 0;
	}

	uint8_t *bitmap = img->bitmap,
	        *endOfRow,
	         widthRemainder = img->w % 4;

	const uint8_t *endOfBitmap = bitmap + img->h * img->stride;

	const uint16_t padding = img->stride - img->w,
	               width32 = img->w / 4;

	uint32_t *bitmap_32,
	         *endOfRow_32;

	while ( bitmap < endOfBitmap ) {
		bitmap_32   = (uint32_t *)bitmap;
		endOfRow_32 = bitmap_32 + width32;
		while ( bitmap_32 < endOfRow_32 ) {
			if ( *bitmap_32++ ) {
				return 1;
			}
		}
		bitmap = (uint8_t *)bitmap_32;
		endOfRow = bitmap + widthRemainder;
		while ( bitmap < endOfRow ) {
			if ( *bitmap++ ) {
				return 1;
			}
		}
		bitmap += padding;
	}
	return 0;
}

static uint8_t processFrame( ASS_Track *assTrack, int frameTime ) {
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
