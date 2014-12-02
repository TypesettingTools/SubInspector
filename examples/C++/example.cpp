#include <cstdio> // sscanf
#include <cstdint>
#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include <sstream>
#include <vector>

extern "C" {
	#include "../../src/ASSInspector.h"
}

struct DialogLine {
	int time;
	std::string line;
};

class ScriptWrapper {
		std::string headerBuffer;
		std::vector<DialogLine> lineBuffers;

	public:
		ScriptWrapper( void );
		void appendHeader( std::string line );
		std::string getHeader( void );
		std::vector<DialogLine> getLines( void );
		void appendDialogue( std::string line, int time );
};

ScriptWrapper::ScriptWrapper( void ) :
headerBuffer( "[Script Info]\n" ) {

}

void ScriptWrapper::appendHeader( std::string line ) {
	headerBuffer += line + "\n";
}

std::string ScriptWrapper::getHeader( void ) {
	return headerBuffer;
}

void ScriptWrapper::appendDialogue( std::string line, int time ) {
	lineBuffers.push_back( {time, line + '\n'} );
}

std::vector<DialogLine> ScriptWrapper::getLines( void ) {
	return lineBuffers;
}

// this should definitely all be one function it is so good
int main( int argc, char **argv ) {
	int width = 0, height = 0;

	if ( 2 > argc ) {
		std::cout << "Usage: " << argv[0] << " infile.ass" << std::endl;
		return 1;
	}

	std::fstream inAss( argv[1], std::ifstream::in );
	if ( inAss.fail( ) ) {
		std::cout << "Failed to open " << argv[1] << std::endl;
	}

	std::string line;
	std::getline( inAss, line );
	if ( "[Script Info]" != line ) {
		// BOM?
		if ( "[Script Info]" != line.substr( 3 ) ) {
			std::cout << "The input file, " << argv[1] << ", does not appear to be valid ASS." << std::endl;
			inAss.close( );
			return 1;
		}
	}

	ScriptWrapper wrapper;

	// This is pretty terrible.
	while ( true ) {
		std::getline( inAss, line );
		if ( '[' == line[0] ) {
			break;
		}
		switch ( line[0] ) {
			case 'S':
				if ( 0 == line.compare( 0, 22, "ScaledBorderAndShadow:" ) ) {
					wrapper.appendHeader( line );
				} else if ( 0 == line.compare( 0, 11, "ScriptType:" ) ) {
					wrapper.appendHeader( line );
				}
				break;
			case 'P':
				if ( 0 == line.compare( 0, 9, "PlayResX:" ) ) {
					std::stringstream converter( line.substr( 9 ) );
					converter >> width;
					wrapper.appendHeader( line );
				} else if ( 0 == line.compare( 0, 9, "PlayResY:" ) ) {
					std::stringstream converter( line.substr( 9 ) );
					converter >> height;
					wrapper.appendHeader( line );
				}
				break;
			case 'W':
				if ( 0 == line.compare( 0, 10, "WrapStyle:" ) ) {
					wrapper.appendHeader( line );
				}
				break;
		}
		if ( !inAss.good( ) ) {
			break;
		}
	}

	wrapper.appendHeader( "[V4+ Styles]" );

	while ( true ) {
		std::getline( inAss, line );
		switch ( line[0] ) {
			case 'S':
				if ( 0 == line.compare( 0, 6, "Style:" ) ) {
					wrapper.appendHeader( line );
				}
				break;
			case 'D':
				if ( 0 == line.compare( 0, 9, "Dialogue:" ) ) {
					int hours = 0, minutes = 0, seconds = 0, centiseconds = 0;
					sscanf( line.substr( 9 ).c_str( ), "%*d,%u:%u:%u.%u", &hours, &minutes, &seconds, &centiseconds );
					int milliseconds = 3600000*hours + 60000*minutes + 1000*seconds + 10*centiseconds;
					wrapper.appendDialogue( line, milliseconds );
				}
				break;
		}
		if ( !inAss.good( ) ) {
			break;
		}
	}

	inAss.close( );

	wrapper.appendHeader( "[Events]" );

	ASSI_State *state = assi_init( width, height, NULL, NULL );

	std::string header = wrapper.getHeader( );
	assi_setHeader( state, header.c_str( ), header.length( ) );

	std::map<uint32_t,int> hashes;
	std::map<uint32_t,int>::iterator match;

	int deleted = 0, kept = 0;
	int processed = 0;

	for ( const auto &pair : wrapper.getLines( ) ) {
		assi_setScript( state, pair.line.c_str( ), pair.line.length( ) );
		ASSI_Rect rect{ 0, 0, 0, 0, 0 };
		assi_calculateBounds( state, &rect, &pair.time, 1 );
		++processed;
		match = hashes.find( rect.hash );
		if ( hashes.end( ) != match ) {
			std::cout << "Line " << processed << ": duplicate hash found (line " << match->second << ")" << std::endl;
		} else {
			hashes[rect.hash] = processed;
		}
		if ( rect.w == 0 || rect.h == 0 ) {
			++deleted;
		} else {
			++kept;
		}
	}

	std::cout << "Deleted " << deleted << " lines out of " << processed << std::endl;

	assi_cleanup( state );
	return 0;
}
