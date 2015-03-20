set(SUBINSPECTOR_SOURCES
	SubInspector.c
)

add_library(SubInspector
	SHARED
		${SUBINSPECTOR_SOURCES}
)


if(ZLIB_FOUND)
	include_directories(${ZLIB_INCLUDE_DIRS})
	target_link_libraries(SubInspector ${ZLIB_LIBRARIES})
else()
	add_definitions(-DNO_ZLIB)
endif()
target_link_libraries(SubInspector ${LIBASS_LIBRARIES})
