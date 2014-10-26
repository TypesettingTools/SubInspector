/* ASSInspector is free software. You can redistribute it and/or modify
 * it under the terms of the MIT license. See COPYING or do a google
 * search for details. */

#include "ASSInspector.h"

#include <stdlib.h>
#include <string.h>
#include <ass/ass.h>

struct ASSI_State_priv {
	ASS_Library  *assLibrary;
	ASS_Renderer *assRenderer;
	char         *header,
	             *currentScript;
	size_t        headerLength,
	              scriptLength;
	ASSI_Rect     lastRect;
	char          error[128];
};

static void checkBounds( ASS_Image*, int32_t*, int32_t* );
static void checkSmallBounds( ASS_Image*, int32_t*, int32_t* );

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

const char* assi_getErrorString( ASSI_State *state ) {
	return state->error;
}

ASSI_State* assi_init( int width, int height, const char* fontConfigConfig, const char *fontDir ) {
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
	ass_set_fonts_dir( state->assLibrary, fontDir );
	ass_set_fonts( state->assRenderer, NULL, "Sans", 1, fontConfigConfig, 1 );

	return state;
}

int assi_setHeader( ASSI_State *state, const char *header ) {
	if ( !state ) {
		return 1;
	}
	// Free the old header before checking the new one for null.
	if ( state->header ) {
		free( state->header );
		state->header = NULL;
		state->headerLength = 0;
	}
	if ( NULL == header ) {
		return 0;
	}
	state->headerLength = strlen( header );
	// Copy terminating null byte too.
	state->header = malloc( state->headerLength + 1 );
	if ( NULL == state->header ) {
		strcpy( state->error, "Memory allocation failure." );
		return 1;
	}
	memcpy( state->header, header, state->headerLength + 1 );
	return 0;
}

int assi_setScript( ASSI_State *state, const char *scriptBody ) {
	if ( !state ) {
		return 1;
	}
	if ( state->currentScript ) {
		free( state->currentScript );
		state->currentScript = NULL;
		state->scriptLength = 0;
	}
	if ( NULL == scriptBody ) {
		return 0;
	}

	size_t scriptBodyLength = strlen( scriptBody );
	state->scriptLength = state->headerLength + scriptBodyLength;
	state->currentScript = malloc( state->scriptLength );
	if ( NULL == state->currentScript ) {
		state->scriptLength = 0;
		strcpy( state->error, "Memory allocation failure." );
		return 1;
	}

	memcpy( state->currentScript, state->header, state->headerLength );
	memcpy( state->currentScript + state->headerLength, scriptBody, scriptBodyLength );

	return 0;
}

int assi_calculateBounds( ASSI_State *state, ASSI_Rect *rects, const int32_t *times, const uint32_t renderCount ) {
	if ( !state ) {
		return 1;
	}

	if ( NULL == state->currentScript ) {
		strcpy( state->error, "currentScript must not be NULL.");
		return 1;
	}

	ASS_Track *assTrack = ass_read_memory( state->assLibrary, state->currentScript, state->scriptLength, NULL );
	if ( NULL == assTrack ) {
		strcpy( state->error, "Memory allocation failure." );
		return 1;
	}

	for ( int i = 0; i < renderCount; ++i ) {
		// set to 1 if positions changed, or set to 2 if content changed.
		int lineChanged = 0;

		ASS_Image *assImage = ass_render_frame( state->assRenderer, assTrack, times[i], &lineChanged );
		if ( NULL == assImage ) {
			continue;
		}

		switch( lineChanged ) {
			// Line is identical to last one.
			case 0:
				rects[i] = state->lastRect;
				continue;

			// The position of the line changed, but its contents are the
			// same. More code duplication!!!!
			case 1:
				rects[i] = state->lastRect;
				rects[i].x = assImage->dst_x;
				rects[i].y = assImage->dst_y;
				assImage = assImage->next;
				while( assImage ) {
					rects[i].x = (assImage->dst_x < rects[i].x)? assImage->dst_x: rects[i].x;
					rects[i].y = (assImage->dst_y < rects[i].y)? assImage->dst_y: rects[i].y;
					assImage = assImage->next;
				}
				state->lastRect = rects[i];
				continue;
		}

		// Set initial conditions.
		rects[i].x = assImage->dst_x;
		rects[i].y = assImage->dst_y;

		int32_t xMax = rects[i].x,
		        yMax = rects[i].y;

		if ( 0xFF != (assImage->color & 0xFF) ) {
			checkBounds( assImage, &xMax, &yMax );
		}
		assImage = assImage->next;

		while ( assImage ) {
			if ( 0xFF != (assImage->color & 0xFF) ) {
				rects[i].x = (assImage->dst_x < rects[i].x)? assImage->dst_x: rects[i].x;
				rects[i].y = (assImage->dst_y < rects[i].y)? assImage->dst_y: rects[i].y;
				checkBounds( assImage, &xMax, &yMax );
			}
			assImage = assImage->next;
		}
		rects[i].w = xMax - rects[i].x;
		rects[i].h = yMax - rects[i].y;
		state->lastRect = rects[i];
	}

	ass_free_track( assTrack );
	return 0;
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

#define SEARCH_BOUNDS 32

static void checkBounds( ASS_Image *assImage, int32_t *xMax, int32_t *yMax ) {
	if ( assImage->w < SEARCH_BOUNDS || assImage->h < SEARCH_BOUNDS ) {
		checkSmallBounds( assImage, xMax, yMax );
		return;
	}
	// Shift back from the end of the first row by SEARCH_BOUNDS bytes.
	uint8_t       *byte = assImage->bitmap + assImage->w - SEARCH_BOUNDS,
	               addHeight;

	uintptr_t     *chunk;

	const uint8_t  chunkSize  = sizeof(*chunk),
	              *start      = assImage->bitmap,
	              *shortEnd   = start + (assImage->h - SEARCH_BOUNDS) * assImage->stride,
	              *realEnd    = shortEnd + (SEARCH_BOUNDS - 1) * assImage->stride + assImage->w,
	               rowPadding = assImage->stride - assImage->w;

	const uint32_t chunksPerRow = assImage->w/chunkSize,
	               chunksPerShortRow = SEARCH_BOUNDS/chunkSize;

	// Process the rightmost SEARCH_BOUNDS bytes of the first height-SEARCH_BOUNDS rows. Because
	// we are guaranteed to be processing SEARCH_BOUNDS bytes here regardless of
	// alignment, don't worry about being careful about chunks here. I
	// don't think anyone has a system that's more than 128-bit.
	while ( byte < shortEnd ) {
		chunk = (uintptr_t *)byte;
		addHeight = 0;

		uintptr_t *endChunk = chunk + chunksPerShortRow;

		uint32_t position = (byte - start),
		         x = position%assImage->stride + assImage->dst_x + 1,
		         y = position/assImage->stride + assImage->dst_y + 1;

		while (chunk < endChunk) {
			if ( *chunk ) {
				// printf( "Chunk: %p; Value: %016lX, End: %p\n", chunk, *chunk, chunk + 1 );
				for ( byte = (uint8_t *)chunk; byte < (uint8_t *)(chunk + 1); byte++ ) {
					if ( *byte ) {
						// printf( "Byte: %p; Value: %u (%3u, %3u)\n", byte, *byte, x, y );
						*xMax = (x > *xMax)? x: *xMax;
						addHeight = 1;
					}
					x++;
				}
			} else {
				x += chunkSize;
			}
			chunk++;
		}
		if ( addHeight ) {
			*yMax = (y > *yMax)? y: *yMax;
		}
		byte = (uint8_t *)chunk + assImage->stride - SEARCH_BOUNDS;
	}

	// Process the bottom SEARCH_BOUNDS rows. Should probably be combined into a
	// function with checkSmallBounds, since they're identical, except the
	// starting pointer and constants for this are already calculated.
	byte = (uint8_t *)shortEnd;

	while ( byte < realEnd ) {
		chunk = (uintptr_t *)byte;
		addHeight = 0;

		uint8_t   *endByte   = byte + assImage->w;

		uintptr_t *endChunk = chunk + chunksPerRow;

		uint32_t   position = (byte - start),
		           x = position%assImage->stride + assImage->dst_x + 1,
		           y = position/assImage->stride + assImage->dst_y + 1;

		while ( chunk < endChunk ) {
			if ( *chunk ) {
				// printf( "Chunk: %p; Value: %016lX, End: %p\n", chunk, *chunk, chunk + 1 );
				for( byte = (uint8_t *)chunk; byte < (uint8_t *)(chunk + 1); byte++ ) {
					if ( *byte ) {
						// printf( "Byte: %p; Value: %u (%3u, %3u)\n", byte, *byte, x, y );
						*xMax = (x > *xMax)? x: *xMax;
						addHeight = 1;
					}
					x++;
				}
			} else {
				x += chunkSize;
			}
			chunk++;
		}

		byte = (uint8_t *)chunk;
		while ( byte < endByte ) {
			if ( *byte ) {
				// printf( "Byte: %p; Value: %u (%3u, %3u)\n", byte, *byte, x, y );
				*xMax = (x > *xMax)? x: *xMax;
				addHeight = 1;
			}
			byte++;
			// Can be lazy about incrementing x here.
			x++;
		}
		if ( addHeight ) {
			*yMax = (y > *yMax)? y: *yMax;
		}
		byte += rowPadding;
	}
}

static void checkSmallBounds( ASS_Image *assImage, int32_t *xMax, int32_t *yMax ) {
	uint8_t       *byte = assImage->bitmap,
	               addHeight;

	uintptr_t     *chunk;

	const uint8_t  chunkSize   = sizeof(*chunk),
	              *bitmapStart = assImage->bitmap,
	              *bitmapEnd   = bitmapStart + (assImage->h - 1) * assImage->stride + assImage->w,
	               rowPadding  = assImage->stride - assImage->w;

	const uint32_t chunksPerRow      = assImage->w/chunkSize;

	// Let's write the same loop 3 times!
	while ( byte < bitmapEnd ) {
		chunk = (uintptr_t *)byte;
		addHeight = 0;

		uint8_t   *endByte   = byte + assImage->w;

		uintptr_t *endChunk = chunk + chunksPerRow;

		uint32_t   position = (byte - bitmapStart),
		           x = position%assImage->stride + assImage->dst_x + 1,
		           y = position/assImage->stride + assImage->dst_y + 1;

		while ( chunk < endChunk ) {
			if ( *chunk ) {
				// printf( "Chunk: %p; Value: %016lX, End: %p\n", chunk, *chunk, chunk + 1 );
				for( byte = (uint8_t *)chunk; byte < (uint8_t *)(chunk + 1); byte++ ) {
					if ( *byte ) {
						// printf( "Byte: %p; Value: %u (%3u, %3u)\n", byte, *byte, x, y );
						*xMax = (x > *xMax)? x: *xMax;
						addHeight = 1;
					}
					x++;
				}
			} else {
				x += chunkSize;
			}
			chunk++;
		}

		byte = (uint8_t *)chunk;
		while ( byte < endByte ) {
			if ( *byte ) {
				// printf( "Byte: %p; Value: %u (%3u, %3u)\n", byte, *byte, x, y );
				*xMax = (x > *xMax)? x: *xMax;
				addHeight = 1;
			}
			byte++;
			// Can be lazy about incrementing x here.
			x++;
		}
		if ( addHeight ) {
			*yMax = (y > *yMax)? y: *yMax;
		}
		byte += rowPadding;
	}
}
