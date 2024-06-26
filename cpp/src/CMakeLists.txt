cmake_minimum_required(VERSION 3.14)
project (h4x_apps)

message(STATUS ${X_INCLUDE_DIRS})
message(STATUS ${X_LIBRARY_DIRS})

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
find_package(MySQL REQUIRED)

include_directories(
	${DB_INCLUDE_DIRS}
	${LOCAL_3RD_INCLUDE_DIR}
	${X_INCLUDE_DIRS}
)

link_directories(
	${DB_LIBRARY_DIRS}
	${LOCAL_3RD_LIBRARY_DIR}
	${X_LIBRARY_DIRS}
)

# message(STATUS ${LOCAL_3RD_LIBRARIES})

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
	maxminddb
	${DB_LIBRARIES} ${MYSQL_LIBRARIES}
	${LOCAL_3RD_LIBRARIES}
	${X_LIBRARIES}
	${PLATFORM_LIBRARIES}
	${cpp})

# message(STATUS ${lib_dependency})

# local libs
aux_source_directory("./common"                    COMMON_SRC)
aux_source_directory("./common_job"                COMMON_SRC)
aux_source_directory("./common_protocol"           COMMON_SRC)
aux_source_directory("./common_server_group"       COMMON_SRC)
aux_source_directory("./common_web_service"        COMMON_SRC)
aux_source_directory("./component"                 COMMON_SRC)
aux_source_directory("./profiler"                  COMMON_SRC)
add_library(app_common ${COMMON_SRC})


# aux_source_directory("../pb/pp2"               PB_SRC)
# add_library(app_pb ${PB_SRC})
# set(lib_dependency PRIVATE ${app_pb} ${lib_dependency})

# applications
function(build_app sub_dir_name)
	aux_source_directory(${sub_dir_name} LOCAL_SOURCE_CODE)
	add_executable(_${sub_dir_name} ${LOCAL_SOURCE_CODE})
	target_compile_definitions(_${sub_dir_name} PRIVATE X_OPTION_STATIC)
	target_link_libraries(_${sub_dir_name} PRIVATE app_common)
	target_link_libraries(_${sub_dir_name} PRIVATE ${lib_dependency})
endfunction()

build_app(server_auth)
build_app(server_dns)
build_app(server_geoip)
build_app(server_proxy)
build_app(server_terminal_relay)

build_app(test)
build_app(test_db)
build_app(test_delay)

install(TARGETS
	_server_auth
	_server_dns
	_server_geoip
	_server_proxy
	_server_terminal_relay

	_test
	_test_db
	_test_delay

	RUNTIME DESTINATION bin
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
)

