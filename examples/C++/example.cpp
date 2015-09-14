#include <cstdio> // sscanf
#include <cstdint>
#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include <sstream>
#include <vector>

extern "C" {
	#include "../../src/SubInspector.h"
}

struct DialogLine {
	int time;
	std::string line;
};

class ScriptWrapper {
		std::string headerBuffer;
		std::fstream &fileStream;
		std::vector<DialogLine> lineBuffers;

	public:
		ScriptWrapper( std::fstream &inFile );
		~ScriptWrapper( void );
		void readFile( int &width, int &height );
		void rewind( void );
		void appendHeader( std::string line );
		std::string& getHeader( void );
		std::vector<DialogLine>& getLines( void );
		void appendDialogue( std::string line, int time );
};

ScriptWrapper::ScriptWrapper( std::fstream &inFile ) :
headerBuffer( "[Script Info]\n" ), fileStream( inFile ) {
}

ScriptWrapper::~ScriptWrapper( void ) {
	fileStream.close( );
}

void ScriptWrapper::readFile( int &width, int &height ) {
	std::string line;
	std::getline( fileStream, line );
	if ( "[Script Info]" != line ) {
		// BOM?
		if ( "[Script Info]" != line.substr( 3 ) ) {
			std::cout << "The input file does not appear to be valid ASS." << std::endl;
			goto fail;
		}
	}

	// This is pretty terrible.
	std::cout << "Reading headers." << std::endl;
	while ( true ) {
		std::getline( fileStream, line );
		if ( '[' == line[0] ) {
			break;
		}
		switch ( line[0] ) {
			case 'S':
				if ( 0 == line.compare( 0, 22, "ScaledBorderAndShadow:" ) ) {
					appendHeader( line );
				} else if ( 0 == line.compare( 0, 11, "ScriptType:" ) ) {
					appendHeader( line );
				}
				break;
			case 'P':
				if ( 0 == line.compare( 0, 9, "PlayResX:" ) ) {
					std::stringstream converter( line.substr( 9 ) );
					converter >> width;
					appendHeader( line );
				} else if ( 0 == line.compare( 0, 9, "PlayResY:" ) ) {
					std::stringstream converter( line.substr( 9 ) );
					converter >> height;
					appendHeader( line );
				}
				break;
			case 'W':
				if ( 0 == line.compare( 0, 10, "WrapStyle:" ) ) {
					appendHeader( line );
				}
				break;
		}
		if ( !fileStream.good( ) ) {
			goto fail;
		}
	}

	appendHeader( "[V4+ Styles]" );

	std::cout << "Reading styles and dialogue." << std::endl;
	while ( !fileStream.eof( ) ) {
		if ( !fileStream.good( ) )
			goto fail;

		std::getline( fileStream, line );
		switch ( line[0] ) {
			case 'S':
				if ( 0 == line.compare( 0, 6, "Style:" ) ) {
					appendHeader( line );
				}
				break;
			case 'D':
				if ( 0 == line.compare( 0, 9, "Dialogue:" ) ) {
					int hours = 0, minutes = 0, seconds = 0, centiseconds = 0;
					sscanf( line.substr( 9 ).c_str( ), "%*d,%u:%u:%u.%u", &hours, &minutes, &seconds, &centiseconds );
					int milliseconds = 3600000*hours + 60000*minutes + 1000*seconds + 10*centiseconds;
					appendDialogue( line, milliseconds );
				}
				break;
		}
	}

	appendHeader( "[Events]" );
	return;

fail:
	fileStream.close( );
	throw;
}

void ScriptWrapper::rewind( void ) {
	fileStream.clear( );
	fileStream.seekg( 0, std::ios::beg );
	if ( !fileStream.good( ) ) {
		std::cout << "WHAt" << std::endl;
	}
}

void ScriptWrapper::appendHeader( std::string line ) {
	headerBuffer += line + "\n";
}

std::string& ScriptWrapper::getHeader( void ) {
	return headerBuffer;
}

void ScriptWrapper::appendDialogue( std::string line, int time ) {
	lineBuffers.push_back( {time, line + '\n'} );
}

std::vector<DialogLine>& ScriptWrapper::getLines( void ) {
	return lineBuffers;
}

// this should definitely all be one function it is so good
int main( int argc, char **argv ) {
	int width = 0, height = 0;

	if ( 2 > argc ) {
		std::cout << "Usage: " << argv[0] << " infile.ass" << std::endl;
		return 1;
	}

	std::fstream inAss( argv[1], std::ios::in | std::ios::binary );
	if ( inAss.fail( ) ) {
		std::cout << "Failed to open " << argv[1] << std::endl;
		return 1;
	}

	ScriptWrapper wrapper( inAss );

	try {
		wrapper.readFile( width, height );
	} catch( ... ) {
		std::cout << "Bad failure is occurred." << std::endl;
		return 1;
	}

	SI_State *state = si_init( width, height, NULL, NULL );

	auto header = wrapper.getHeader( );
	si_setHeader( state, header.c_str( ), header.length( ) );

	std::map<uint32_t,int> hashes;
	std::map<uint32_t,int>::iterator match;

	int deleted = 0, kept = 0;
	int processed = 0;

	for ( const auto &pair : wrapper.getLines( ) ) {
		si_setScript( state, pair.line.c_str( ), pair.line.length( ) );
		SI_Rect rect{ 0, 0, 0, 0, 0 };
		si_calculateBounds( state, &rect, &pair.time, 1 );
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

	si_cleanup( state );
	return 0;
}
