cmake_minimum_required(VERSION 3.14)
project (pp_apps)

include_directories(
	../src_lib_common/
	../src_lib_db/
	../src_lib_protocol/
	${LOCAL_3RD_INCLUDE_DIR}
	${X_INCLUDE_DIRS}
)

link_directories(
	${LIBRARY_OUTPUT_PATH}
	${LOCAL_3RD_LIBRARY_DIR}
	${X_LIBRARY_DIRS}
)

message(STATUS ${LIBRARY_OUTPUT_PATH})
set(LOCAL_PP_LIBRARIES ppprotocol ppdb ppcommon)

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

aux_source_directory("./lib_backend_connection"      LIB_SRC)
aux_source_directory("./lib_client"                  LIB_SRC)
aux_source_directory("./lib_util"                    LIB_SRC)

add_library(server ${LIB_SRC})
target_compile_definitions(server PRIVATE X_OPTION_STATIC)

set(lib_dependency
	server
	${LOCAL_PP_LIBRARIES}
	${LOCAL_3RD_LIBRARIES}
	${X_LIBRARIES}
	${PLATFORM_LIBRARIES}
	${cpp})

# applications
function(build_app sub_dir_name)
	aux_source_directory(${sub_dir_name} LOCAL_SOURCE_CODE)
	add_executable(${sub_dir_name} ${LOCAL_SOURCE_CODE})
	target_compile_definitions(${sub_dir_name} PRIVATE X_OPTION_STATIC)
	target_link_libraries(${sub_dir_name} PRIVATE ${lib_dependency})
endfunction()

set(target_list

	s_server_id_center
	s_server_list

	s_address_test
	s_config_center
	s_device_state_relay
	s_relay
	s_relay_id_client
	s_relay_id_manager
	s_relay_info_dispatcher

	############ tests:
	t_lang
	t_server_list_client

	############ other:
	)



foreach(item IN LISTS target_list)
    build_app(${item})
endforeach()

set(LIBRDKAFKA_LIBRARIES librdkafka++.a librdkafka-static.a libssl.a libcrypto.a)
# target_link_libraries(s_audit_account PRIVATE ${LIBRDKAFKA_LIBRARIES})
# target_link_libraries(r_device_audit_to_backend PRIVATE ${LIBRDKAFKA_LIBRARIES})

install(TARGETS
	${target_list}

	RUNTIME DESTINATION bin
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
)

