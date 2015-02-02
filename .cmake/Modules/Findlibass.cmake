# Handle QUIET and REQUIRED etc.
include(FindPackageHandleStandardArgs)

# Just use pkg-config for hints.
find_package(PkgConfig QUIET)
if (PKGCONFIG_FOUND)
	pkg_check_modules(PC_LIBASS QUIET libass)
endif()

# locate <ass/ass.h>
find_path(LIBASS_INCLUDE_DIRS
	NAMES
		ass/ass.h
	HINTS
		${PC_LIBASS_INCLUDEDIR}
		${PC_LIBASS_INCLUDE_DIRS}
	PATHS
		${LIBASS_INC_DIR}
		$ENV{LIBASS_INC_DIR}
		/usr/local/include
		/usr/include
)

# locate libass.(dylib|so)
find_library(LIBASS_LIBRARIES
	NAMES
		ass
	HINTS
		${PC_LIBASS_LIBDIR}
		${PC_LIBASS_LIBRARY_DIRS}
	PATHS
		${LIBASS_LIB_DIR}
		$ENV{LIBASS_LIB_DIR}
		/usr/local/lib
		/usr/local/lib64
		/usr/local/lib/${CMAKE_LIBRARY_ARCHITECTURE}
		/usr/lib
		/usr/lib64
		/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}
)

# Error if header or library not found.
find_package_handle_standard_args(libass
	"libass not found. If you have installed it in a custom location, try setting the variables LIBASS_INC_DIR and LIBASS_LIB_DIR."

	LIBASS_LIBRARIES
	LIBASS_INCLUDE_DIRS
)
