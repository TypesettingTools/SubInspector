/* SubInspector is free software. You can redistribute it and/or modify
 * it under the terms of the MIT license. See COPYING or do a google
 * search for details. */

#include "SubInspector.h"

#include <stdlib.h>
#include <string.h>
#include <ass/ass.h>

// FreeType links against zlib by default if it is available. We bundle
// our own drop-in solution just in case people don't have zlib or don't
// want to link against it. zlib's crc32 is faster than this one because
// it is significantly more riced, but it shouldn't matter for the small
// amount of data being hashed.
#ifdef NO_ZLIB
	#include "crc32table.h"
	// COPYRIGHT (C) 1986 Gary S. Brown.  You may use this program, or
	// code or tables extracted from it, as desired without restriction.
	// Modified from: http://www.opensource.apple.com/source/xnu/xnu-1456.1.26/bsd/libkern/crc32.c
	static uint32_t crc32( uint32_t crc, const void *buf, size_t size ) {
		const uint8_t *p;

		p = buf;
		crc = crc ^ ~0U;

		// zlib checksum unrolls this loop into 8-byte chunks, but that
		// doesn't seem to make a major performance difference for reasonably
		// sized buffers.
		while ( size-- ) {
			crc = crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
		}

		return crc ^ ~0U;
	}
#else
	#include <zlib.h>
#endif // NO_ZLIB

struct SI_State_priv {
	ASS_Library  *assLibrary;
	ASS_Renderer *assRenderer;
	char         *header,
	             *currentScript;
	size_t        headerLength,
	              scriptLength;
	SI_Rect       lastRect;
	char          error[128];
};

typedef struct {
	int x1, y1, x2, y2;
} SI_InternalRect;

static uint8_t checkBounds( ASS_Image*, SI_InternalRect*, uint32_t* );

static void msgCallback( int level, const char *fmt, va_list va, void *data ) {
	if ( level < 4 ) {
		SI_State *state = data;
		int levelLength = sprintf( state->error, "%d: ", level );
		vsnprintf( state->error + levelLength, sizeof(state->error) - levelLength, fmt, va );
	}
}

uint32_t si_getVersion( void ) {
	return SI_VERSION;
}

const char* si_getErrorString( SI_State *state ) {
	return state->error;
}

SI_State* si_init( int width, int height, const char *fontConfigConfig, const char *fontDir ) {
	SI_State *state = calloc( 1, sizeof(*state) );
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

	ass_set_cache_limits(state->assRenderer, 1000, 1000 );
	si_changeResolution( state, width, height );
	si_reloadFonts( state, fontConfigConfig, fontDir );

	return state;
}

void si_changeResolution( SI_State *state, int width, int height ) {
	if ( NULL == state ) {
		return;
	}
	ass_set_frame_size( state->assRenderer, width, height );
}

void si_reloadFonts( SI_State *state, const char *fontConfigConfig, const char *fontDir ) {
	if ( NULL == state ) {
		return;
	}
	ass_set_fonts_dir( state->assLibrary, fontDir );
	ass_set_fonts( state->assRenderer, NULL, "Sans", 1, fontConfigConfig, 1 );
}

int si_setHeader( SI_State *state, const char *header, size_t headerLength ) {
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

	if ( headerLength ) {
		state->headerLength = headerLength;
	} else {
		state->headerLength = strlen( header );
	}

	state->header = malloc( state->headerLength );
	if ( NULL == state->header ) {
		strcpy( state->error, "Memory allocation failure." );
		return 1;
	}
	memcpy( state->header, header, state->headerLength );
	return 0;
}

int si_setScript( SI_State *state, const char *scriptBody, size_t bodyLength ) {
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

	size_t scriptBodyLength = 0;
	if ( bodyLength ) {
		scriptBodyLength = bodyLength;
	} else {
		scriptBodyLength = strlen( scriptBody );
	}

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

int si_calculateBounds( SI_State *state, SI_Rect *rects, const int32_t *times, const uint32_t renderCount ) {
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

			// dst_x and dst_y aren't actually trustworthy, so this can get
			// chucked right out the window. It'd still be possible to
			// optimize the moved case by storing the offset between dst_[xy]
			// and the actual bounding rect.
		}

		// Set initial conditions.

		SI_InternalRect boundsRect = {
			assImage->dst_x + assImage->w,
			assImage->dst_y + assImage->h,
			0,
			0
		};

		uint8_t solid = 0;
		uint32_t pixelHash = 0;

		do {
			if ( assImage->h == 0 || assImage->w  == 0 ) {
				assImage = assImage->next;
				continue;
			}
			if ( 0xFF != (assImage->color & 0xFF) ) {
				solid = solid | checkBounds( assImage, &boundsRect, &pixelHash );
			}
			pixelHash = crc32( pixelHash, (void*)&assImage->color, sizeof(assImage->color) );

			assImage = assImage->next;
		} while ( assImage );

		boundsRect.x2 = (boundsRect.x2 < boundsRect.x1)? boundsRect.x1: boundsRect.x2;
		boundsRect.y2 = (boundsRect.y2 < boundsRect.y1)? boundsRect.y1: boundsRect.y2;

		rects[i].x = boundsRect.x1;
		rects[i].y = boundsRect.y1;
		rects[i].w = boundsRect.x2 - boundsRect.x1;
		rects[i].h = boundsRect.y2 - boundsRect.y1;
		rects[i].solid = solid;
		rects[i].hash = crc32( pixelHash, (void*)&rects[i], sizeof(rects[i]) );

		state->lastRect = rects[i];
	}

	ass_free_track( assTrack );
	return 0;
}

void si_cleanup( SI_State *state ) {
	if ( state ) {
		free( state->header );
		free( state->currentScript );
		ass_renderer_done( state->assRenderer );
		ass_library_done( state->assLibrary );
		free( state );
	}
}

static uint8_t checkBounds( ASS_Image *assImage, SI_InternalRect *boundsRect, uint32_t *pixelHash ) {
	uint8_t       *byte = assImage->bitmap,
	               addHeight,
	               solid = 0;

	uintptr_t     *chunk;

	const uint8_t  chunkSize   = sizeof(*chunk),
	              *bitmapStart = assImage->bitmap,
	              *bitmapEnd   = assImage->bitmap + (assImage->h - 1) * assImage->stride + assImage->w;
	const uint32_t rowPadding  = assImage->stride - assImage->w;

	const uint32_t chunksPerRow      = assImage->w/chunkSize;

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
						*pixelHash = crc32( *pixelHash, (void *)byte, 1 );
						if ( *byte == 255 ) {
							solid = 1;
						}
						// printf( "Byte: %p; Value: %u (%3u, %3u)\n", byte, *byte, x, y );
						if ( x <= boundsRect->x1 ) {
							boundsRect->x1 = x - 1;
						}
						if ( x > boundsRect->x2 ) {
							boundsRect->x2 = x;
						}
					}
					x++;
				}
				addHeight = 1;
			} else {
				x += chunkSize;
			}
			chunk++;
		}

		byte = (uint8_t *)chunk;
		while ( byte < endByte ) {
			if ( *byte ) {
				*pixelHash = crc32( *pixelHash, (void *)byte, 1 );

				if ( *byte == 255 ) {
					solid = 1;
				}
				// printf( "Byte: %p; Value: %u (%3u, %3u)\n", byte, *byte, x, y );
				if ( x <= boundsRect->x1 ) {
					boundsRect->x1 = x - 1;
				}
				if ( x > boundsRect->x2 ) {
					boundsRect->x2 = x;
				}
				addHeight = 1;
			}
			byte++;
			// Can be lazy about incrementing x here.
			x++;
		}
		if ( addHeight ) {
			if ( y <= boundsRect->y1 ) {
				boundsRect->y1 = y - 1;
			}
			if ( y > boundsRect->y2 ) {
				boundsRect->y2 = y;
			}
		}
		byte += rowPadding;
	}
	return solid;
}
