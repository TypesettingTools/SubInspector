#pragma once

#include <stdint.h>

int  optimASS_init( int, int );
void optimASS_addHeader( const char*, unsigned int );
void optimASS_initEvents( unsigned int );
void optimASS_addEvent( const char*, unsigned int, unsigned int );
int  optimASS_checkLine( const int, const int*, const unsigned int, uint8_t* );
void optimASS_cleanup( void );
