cmake_minimum_required(VERSION 3.14)
set(libname ppprotocol)
project (lib_${libname})

include_directories(
	../src_lib_common/
	${LOCAL_3RD_INCLUDE_DIR}
	${X_INCLUDE_DIRS}
)

# local libs
aux_source_directory("./pp_protocol"                        SRC)
aux_source_directory("./pp_protocol/_backend"               SRC)
aux_source_directory("./pp_protocol/_backend/json"          SRC)
aux_source_directory("./pp_protocol/detail"                 SRC)
aux_source_directory("./pp_protocol/device"                 SRC)
aux_source_directory("./pp_protocol/internal"               SRC)

add_library(${libname} ${SRC})
target_compile_definitions(${libname} PRIVATE X_OPTION_STATIC)

install(
	DIRECTORY ${PROJECT_SOURCE_DIR}/
	DESTINATION include
	FILES_MATCHING
		PATTERN "*.h"
		PATTERN "*.hpp"
	)

install(
	TARGETS
	${libname}
	RUNTIME DESTINATION bin
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
)

