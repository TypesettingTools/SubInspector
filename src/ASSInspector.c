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
	uint32_t      headerLength,
	              scriptLength;
	char          error[128];
};

static void msgCallback( int, const char*, va_list, void* );
static void checkBounds( ASS_Image*, ASSI_Rect* );
static void checkSmallBounds( ASS_Image*, ASSI_Rect* );

static void msgCallback( int level, const char *fmt, va_list va, void *data ) {
	if ( level < 4 ) {
		ASSI_State *state = data;
		int levelLength = sprintf( state->error, "%d: ", level );
		vsnprintf( state->error + levelLength, sizeof(state->error) - levelLength, fmt, va );
	}
}

ASSI_EXPORT uint32_t assi_getVersion( void ) {
	return ASSI_VERSION;
}

ASSI_EXPORT const char* assi_getErrorString( ASSI_State *state ) {
	return state->error;
}

ASSI_EXPORT ASSI_State* assi_init( int width, int height, const char *header, uint32_t headerLength, const char* fontConfigConfig ) {
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
	ass_set_fonts( state->assRenderer, NULL, "Sans", 1, fontConfigConfig, 1 );

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

// There doesn't seem to be any way to get error reporting from these
// functions, so there's no reason to return a status code.
ASSI_EXPORT void assi_setFontDir( ASSI_State *state, const char *directory ) {
	ass_set_fonts_dir( state->assLibrary, directory );
	ass_fonts_update( state->assRenderer );
}

ASSI_EXPORT int assi_setScript( ASSI_State *state, const char *styles, uint32_t stylesLength, const char *events, const uint32_t eventsLength ) {
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

ASSI_EXPORT int assi_calculateBounds( ASSI_State *state, ASSI_Rect *rects, const int32_t *times, const uint32_t renderCount ) {
	if ( !state ) {
		return 1;
	}

	ASS_Track *assTrack = ass_read_memory( state->assLibrary, state->currentScript, state->scriptLength, NULL );
	if ( NULL == assTrack ) {
		return 1;
	}

	for ( int i = 0; i < renderCount; ++i ) {
		ASS_Image *assImage = ass_render_frame( state->assRenderer, assTrack, times[i], NULL );
		if ( NULL == assImage ) {
			return 0;
		}
		rects[i].x = assImage->dst_x;
		rects[i].y = assImage->dst_y;
		while ( assImage ) {
			if ( 0xFF != (assImage->color & 0xFF) ) {
				checkBounds( assImage, &rects[i] );
			}
			assImage = assImage->next;
		}
	}

	ass_free_track( assTrack );
	return 0;
}

ASSI_EXPORT void assi_cleanup( ASSI_State *state ) {
	if ( state ) {
		free( state->header );
		free( state->currentScript );
		ass_renderer_done( state->assRenderer );
		ass_library_done( state->assLibrary );
		free( state );
	}
}

static void checkBounds( ASS_Image *assImage, ASSI_Rect *rect ) {
	if ( assImage->w < 16 || assImage->h < 16 ) {
		checkSmallBounds( assImage, rect );
		return;
	}
	// Shift back from the end of the first row by 16 bytes.
	// Theoretically only needs to be 15 because if all 15 are blank, it
	// means that the one before that must be the boundary of the image.
	// But this is easier.
	uint8_t       *byte = assImage->bitmap + assImage->w - 16;

	uintptr_t     *chunk;

	const uint8_t  chunkSize  = sizeof(*chunk),
	              *start      = assImage->bitmap,
	              *shortEnd   = start + (assImage->h - 16) * assImage->stride,
	              *realEnd    = shortEnd + 15 * assImage->stride + assImage->w,
	               rowPadding = assImage->stride - assImage->w;

	const uint32_t chunksPerRow = assImage->w/chunkSize,
	               chunksPerShortRow = 16/chunkSize;

	// Process the rightmost 16 bytes of the first height-16 rows. Because
	// we are guaranteed to be processing 16 bytes here regardless of
	// alignment, don't worry about being careful about chunks here. I
	// don't think anyone has a system that's more than 128-bit.
	while ( byte < shortEnd ) {
		chunk = (uintptr_t *)byte;
		uintptr_t *endChunk = chunk + chunksPerShortRow;

		uint32_t position = (byte - start),
		         x = position%assImage->stride + 1,
		         y = position/assImage->stride + 1;
		while (chunk < endChunk) {
			if ( *chunk ) {
				// printf( "Chunk: %p; Value: %016lX, End: %p\n", chunk, *chunk, chunk + 1 );
				for ( byte = (uint8_t *)chunk; byte < (uint8_t *)(chunk + 1); byte++ ) {
					if ( *byte ) {
						// printf( "Byte: %p; Value: %u (%3u, %3u)\n", byte, *byte, x, y );
						rect->w = (x > rect->w)? x: rect->w;
					}
					x++;
				}
				rect->h = (y > rect->h)? y: rect->h;
			}
			chunk++;
			position = ((uint8_t*)chunk - start);
			x = position%assImage->stride + 1;
		}
		byte = (uint8_t *)chunk + assImage->stride - 16;
	}

	// Process the bottom 16 rows. Should probably be combined into a
	// function with checkSmallBounds, since they're identical, except the
	// starting pointer and constants for this are already calculated.
	byte = (uint8_t *)shortEnd;

	while ( byte < realEnd ) {
		chunk = (uintptr_t *)byte;

		uint8_t   *endByte   = byte + assImage->w,
		           addHeight = 0;

		uintptr_t *endChunk = chunk + chunksPerRow;

		uint32_t   position = (byte - start),
		           x = position%assImage->stride + 1,
		           y = position/assImage->stride + 1;

		while ( chunk < endChunk ) {
			if ( *chunk ) {
				// printf( "Chunk: %p; Value: %016lX, End: %p\n", chunk, *chunk, chunk + 1 );
				for( byte = (uint8_t *)chunk; byte < (uint8_t *)(chunk + 1); byte++ ) {
					if ( *byte ) {
						// printf( "Byte: %p; Value: %u (%3u, %3u)\n", byte, *byte, x, y );
						rect->w = (x > rect->w)? x: rect->w;
						addHeight = 1;
					}
					x++;
				}
			}
			chunk++;
			position = ((uint8_t*)chunk - start);
			x = position%assImage->stride + 1;
		}

		byte = (uint8_t *)chunk;
		while ( byte < endByte ) {
			if ( *byte ) {
				// printf( "Byte: %p; Value: %u (%3u, %3u)\n", byte, *byte, x, y );
				rect->w = (x > rect->w)? x: rect->w;
				addHeight = 1;
			}
			byte++;
			// Can be lazy about incrementing x here.
			x++;
		}
		rect->h = addHeight? y: rect->h;
		byte += rowPadding;
	}
}

static void checkSmallBounds( ASS_Image *assImage, ASSI_Rect *rect ) {
	uint8_t       *byte = assImage->bitmap;

	uintptr_t     *chunk;

	const uint8_t  chunkSize   = sizeof(*chunk),
	              *bitmapStart = assImage->bitmap,
	              *bitmapEnd   = bitmapStart + (assImage->h - 1) * assImage->stride + assImage->w,
	               rowPadding  = assImage->stride - assImage->w;

	const uint32_t chunksPerRow      = assImage->w/chunkSize;

	// Let's write the same loop 3 times!
	while ( byte < bitmapEnd ) {
		chunk = (uintptr_t *)byte;

		uint8_t   *endByte   = byte + assImage->w,
		           addHeight = 0;

		uintptr_t *endChunk = chunk + chunksPerRow;

		uint32_t   position = (byte - bitmapStart),
		           x = position%assImage->stride + 1,
		           y = position/assImage->stride + 1;

		while ( chunk < endChunk ) {
			if ( *chunk ) {
				// printf( "Chunk: %p; Value: %016lX, End: %p\n", chunk, *chunk, chunk + 1 );
				for( byte = (uint8_t *)chunk; byte < (uint8_t *)(chunk + 1); byte++ ) {
					if ( *byte ) {
						// printf( "Byte: %p; Value: %u (%3u, %3u)\n", byte, *byte, x, y );
						rect->w = (x > rect->w)? x: rect->w;
						addHeight = 1;
					}
					x++;
				}
			}
			chunk++;
			position = ((uint8_t*)chunk - bitmapStart);
			x = position%assImage->stride + 1;
		}

		byte = (uint8_t *)chunk;
		while ( byte < endByte ) {
			if ( *byte ) {
				// printf( "Byte: %p; Value: %u (%3u, %3u)\n", byte, *byte, x, y );
				rect->w = (x > rect->w)? x: rect->w;
				addHeight = 1;
			}
			byte++;
			// Can be lazy about incrementing x here.
			x++;
		}
		rect->h = addHeight? y: rect->h;
		byte += rowPadding;
	}
}
