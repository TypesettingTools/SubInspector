#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ass/ass.h>

#include "OptimASS.h"

static uint8_t findDirty( ASS_Image* );
static uint8_t processFrame( ASS_Track*, int );

static ASS_Library  *assLibrary;
static ASS_Renderer *assRenderer;
static char         *header;
static unsigned int  headerLength;
static char        **lines;
static unsigned int *lineLengths;
static unsigned int  lineCount;

int optimASS_init( int width, int height ) {
	assLibrary = ass_library_init( );
	if ( NULL == assLibrary) {
		puts( "Failed to initialize assLibrary." );
		return 1;
	}

	assRenderer = ass_renderer_init( assLibrary );
	if ( NULL == assRenderer ) {
		puts( "Failed to initialize assRenderer." );
		return 1;
	}

	ass_set_frame_size( assRenderer, width, height );
	// Set fallback fonts
	ass_set_fonts( assRenderer, NULL, "Sans", 1, NULL, 1 );
	return 0;
}

void optimASS_addHeader( char *newHeader, unsigned int length ) {
	header = newHeader;
	headerLength = length;
}

void optimASS_addEvents( char **events, unsigned int *lengths, unsigned int count ) {
	lines = events;
	lineLengths = lengths;
	lineCount = count;
}

void optimASS_cleanup( void ) {
	ass_renderer_done( assRenderer );
	ass_library_done( assLibrary );
}

// Takes the index (0-based) of the line to render and an array of times
// (corresponding to the frames of the line). Returns an array of
// zeros/ones, each corresponding to a frame in the input array.
uint8_t* optimASS_checkLine( const int lineIndex, const int *times, const unsigned int timesLength ) {
	uint8_t *result = calloc( timesLength, sizeof *result );

	// Merge the header and the desired line. None of this needs to be
	// null terminated because we have the length of everything.
	int scriptLength = headerLength + lineLengths[lineIndex];
	char *script = malloc( scriptLength * sizeof *script );
	memcpy( script, header, headerLength );
	memcpy( script + headerLength, lines[lineIndex], lineLengths[lineIndex] );

	ASS_Track *assTrack = ass_read_memory( assLibrary, script, scriptLength, NULL );
	if ( NULL == assTrack ) {
		puts( "Failed to read ASS file." );
		return NULL;
	}

	for ( int timeIdx = 0; timeIdx < timesLength; ++ timeIdx ) {
		result[timeIdx] = processFrame( assTrack, times[timeIdx] );
	}

	ass_free_track( assTrack );

	return result;
}

static uint8_t findDirty( ASS_Image *img ) {
	// If alpha is 255, the image is fully transparent and we can
	// determine it is not dirty immediately.
	if ( 0xFF == (img->color & 0xFF) ) {
		return 0;
	}

	uint8_t *src = img->bitmap;
	for ( int row = 0; row < img->h; ++row ) {
		for ( int col = 0; col < img->w; ++col ) {
			// Any source pixels with a value > 0 are dirty. If even one pixel
			// is found to be dirty, the frame is considered to be dirty.
			if ( src[col] > 0 ) {
				return 1;
			}
		}
		src += img->stride;
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
