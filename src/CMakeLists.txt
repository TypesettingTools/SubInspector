set(ASSINSPECTOR_SOURCES
	ASSInspector.c
)

add_library(ASSInspector
	SHARED
		${ASSINSPECTOR_SOURCES}
)


if(ZLIB_FOUND)
	include_directories(${ZLIB_INCLUDE_DIRS})
	target_link_libraries(ASSInspector ${ZLIB_LIBRARIES})
else()
	add_definitions(-DNO_ZLIB)
endif()
target_link_libraries(ASSInspector ${LIBASS_LIBRARIES})
