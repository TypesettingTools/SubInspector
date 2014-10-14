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
		if ( NULL == assImage ) {
			return 0;
		}
		// ASS_Image is apparently a linked list.
		rects[i].x = assImage->dst_x;
		rects[i].y = assImage->dst_y;
		while ( assImage ) {
			checkBounds( assImage, &rects[i] );
			assImage = assImage->next;
		}
	}

	ass_free_track( assTrack );
	return 0;
}

// Currently assumes that assImage->w is at least mod8 on 64-bit
// platforms. This is not actually true on lines that have clips on
// them, but should be true on pretty much anything else.
static void checkBounds( ASS_Image *assImage, ASSI_Rect *rect ) {
	if ( assImage->w < 16 || assImage->h < 16 ) {
		// checkSmallBounds( ASS_Image, rect );
	}
	if ( 0xFF != (assImage->color & 0xFF) ) {
		// Shift back from the end of the first row by 16 bytes.
		// Theoretically only needs to be 15 because if all 15 are blank, it
		// means that the one before that must be the boundary of the image.
		// But this is easier.
		uint8_t       *byte = assImage->bitmap + assImage->w - 16;

		uintptr_t     *chunk;

		const uint8_t  chunkSize = sizeof(*chunk),
		              *start     = assImage->bitmap,
		              *shortEnd  = start + (assImage->h - 16) * assImage->stride,
		              *realEnd   = shortEnd + 15 * assImage->stride + assImage->w,
		               padding   = assImage->stride - assImage->w;

		const uint32_t chunksPerRow = assImage->w/chunkSize,
		               chunksPerShortRow = 16/chunkSize;

		// Process the rightmost 16 bytes of the first height-16 rows.
		while ( byte < shortEnd ) {
			chunk = (uintptr_t *)byte;
			uintptr_t *endChunk = chunk + chunksPerShortRow;

			uint32_t position = (byte - start),
			         x = position%assImage->stride + 1,
			         y = position/assImage->stride + 1;
			while (chunk < endChunk) {
				if ( *chunk ) {
					for ( byte = (uint8_t *)chunk; byte < (uint8_t *)(chunk + 1); byte++ ) {
						if ( *byte ) {
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

		// Process the bottom 16 rows.
		byte = (uint8_t *)shortEnd;

		while ( byte < realEnd ) {
			chunk = (uintptr_t *)byte;
			uintptr_t *endChunk = chunk + chunksPerRow;
			uint32_t position = (byte - start),
			         x = position%assImage->stride + 1,
			         y = position/assImage->stride + 1;
			while ( chunk < endChunk ) {
				if ( *chunk ) {
					for( byte = (uint8_t *)chunk; byte < (uint8_t *)(chunk + 1); byte++ ) {
						if ( *byte ) {
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
			byte = (uint8_t *)chunk + padding;
		}
	}
}
