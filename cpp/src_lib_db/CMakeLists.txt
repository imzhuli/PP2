cmake_minimum_required(VERSION 3.14)
set(libname ppdb)
project (lib_${libname})

message(STATUS ${X_INCLUDE_DIRS})
message(STATUS ${X_LIBRARY_DIRS})

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/" )
find_package(MySQL REQUIRED)

include_directories(
	${MYSQL_INCLUDE_DIR}
	${X_INCLUDE_DIRS}
)

link_directories(
	${X_LIBRARY_DIRS}
)

# enable specific macors by system type:
if (CMAKE_SYSTEM_NAME MATCHES "Windows")
	set(PLATFORM_LIBRARIES ws2_32 Mswsock)
elseif (CMAKE_SYSTEM_NAME MATCHES "Linux")
	set(PLATFORM_LIBRARIES dl)
elseif (CMAKE_SYSTEM_NAME MATCHES "Darwin")
	find_library(COCOA_LIBRARY Cocoa REQUIRED)
	find_library(IOKIT_LIBRARY IOKit REQUIRED)
	find_library(FUNDATION_LIBRARY Foundation REQUIRED)
	find_library(SYSTEM_CONFIGURATION_LIBRARY SystemConfiguration REQUIRED)
	set(PLATFORM_LIBRARIES dl ${SYSTEM_CONFIGURATION_LIBRARY} ${FUNDATION_LIBRARY} ${IOKIT_LIBRARY} ${COCOA_LIBRARY} )
endif()

set(lib_dependency
	${MYSQL_LIBRARIES}
	${X_LIBRARIES}
	${PLATFORM_LIBRARIES}
	${cpp})

aux_source_directory(./ COMMON_SRC)

add_library(${libname} ${COMMON_SRC})
target_compile_definitions(${libname} PRIVATE X_OPTION_STATIC)

function(build_app sub_dir_name)
	aux_source_directory(${sub_dir_name} LOCAL_SOURCE_CODE)
	add_executable(_${sub_dir_name} ${LOCAL_SOURCE_CODE})
	target_compile_definitions(_${sub_dir_name} PRIVATE X_OPTION_STATIC)
	target_link_libraries(_${sub_dir_name} PRIVATE ${libname} ${lib_dependency})
endfunction()

install(
	DIRECTORY ${PROJECT_SOURCE_DIR}/_include/
	DESTINATION include
	FILES_MATCHING
		PATTERN "*.h"
		PATTERN "*.hpp"
	)

install(TARGETS
	${libname}
	RUNTIME DESTINATION bin
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
	)

