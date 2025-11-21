cmake_minimum_required(VERSION 3.14)
project (h4x)

enable_language(CXX)

message(STATUS "CMAKE_BUILD_TYPE=" ${CMAKE_BUILD_TYPE})
message(STATUS "CMAKE_SYSTEM_NAME=" ${CMAKE_SYSTEM_NAME})
message(STATUS "CMAKE_CXX_COMPILER_ID=" ${CMAKE_CXX_COMPILER_ID})

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

find_package(Threads REQUIRED)
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	add_compile_options("-D_CRT_SECURE_NO_WARNINGS")
	add_compile_options("-std:c++20")
	if (CMAKE_BUILD_TYPE STREQUAL "Release")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
	endif()
	set(cpp Threads::Threads)
else()
	set(CMAKE_CXX_STANDARD 20)
	set(CMAKE_CXX_STANDARD_REQUIRED ON)
	set(CMAKE_CXX_EXTENSIONS OFF)

	if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		add_compile_options("-g")
		add_compile_options("-Wall")
		add_compile_options("-Wno-attributes")
		add_compile_options("-Wno-parentheses")
		add_compile_options("-Wno-deprecated-declarations")
		add_compile_options("-Werror")
		set(cpp rt stdc++fs stdc++ Threads::Threads)
	elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
		add_compile_options("-g")
		add_compile_options("-Wall")
		add_compile_options("-Wno-attributes")
		add_compile_options("-Wno-parentheses")
		add_compile_options("-Wno-deprecated-declarations")
		add_compile_options("-Werror")
		set(cpp c++fs c++ Threads::Threads)
	elseif (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
		add_compile_options("-g")
		add_compile_options("-Wall")
		add_compile_options("-Wno-attributes")
		add_compile_options("-Wno-parentheses")
		add_compile_options("-Wno-deprecated-declarations")
		add_compile_options("-Werror")
		set(cpp Threads::Threads)
	endif()
endif()

if (CMAKE_SYSTEM_NAME MATCHES "MSYS")
	if (CMAKE_BUILD_TYPE STREQUAL "Debug")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mconsole")
	endif()
	include_directories("/mingw64/include/")
	link_directories("/mingw64/lib/")
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
	include_directories("/usr/local/include/")
	link_directories("/usr/local/lib/")
endif()

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
		set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/.local" CACHE PATH "..." FORCE)
	else()
		set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/.local" CACHE PATH "..." FORCE)
	endif()
endif()

set(LIBRARY_OUTPUT_PATH                        "${PROJECT_BINARY_DIR}/lib")
set(EXECUTABLE_OUTPUT_PATH                     "${PROJECT_BINARY_DIR}/bin")
set(CMAKE_MODULE_PATH                          ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/Modules/")

set(CMAKE_BUILD_RPATH_USE_ORIGIN               TRUE)
set(CMAKE_INSTALL_REMOVE_ENVIRONMENT_RPATH     TRUE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH          FALSE)
set(CMAKE_INSTALL_RPATH                        "\$ORIGIN")

if ("${X_LIB}" STREQUAL "")
	message(FATAL_ERROR "x_lib dependency not found")
else()
	set(X_INCLUDE_DIRS "${X_LIB}/./_3rd_installed/include" "${X_LIB}/./_corex_installed/include")
	set(X_LIBRARY_DIRS "${X_LIB}/./_3rd_installed/lib" "${X_LIB}/./_corex_installed/lib")
	set(X_LIBRARIES     xel curl websockets everest freetype mbedtls mbedx509 mbedcrypto p256m z)
endif()

set(LOCAL_3RD_DIR          "${PROJECT_SOURCE_DIR}/../_3rd_installed")
set(LOCAL_3RD_INCLUDE_DIR  "${LOCAL_3RD_DIR}/include")
set(LOCAL_3RD_LIBRARY_DIR  "${LOCAL_3RD_DIR}/lib")
set(LOCAL_3RD_CMAKE_DIR    "${LOCAL_3RD_LIBRARY_DIR}/cmake")

# abseil & protobuf
# set(LOCAL_3RD_LIBRARIES    protobuf protobuf-lite utf8_range utf8_validity)
# set(LOCAL_3RD_LIBRARIES
# 	${LOCAL_3RD_LIBRARIES}
# 	absl_cord absl_cordz_functions absl_cordz_info absl_cordz_sample_token absl_cord_internal absl_cordz_handle
# 	absl_crc_cord_state absl_crc32c absl_crc_cpu_detect absl_crc_internal
#
# 	absl_flags_commandlineflag absl_flags_commandlineflag_internal absl_flags_config
# 	absl_flags_internal absl_flags_marshalling absl_flags_parse absl_flags_private_handle_accessor absl_flags_program_name
# 	absl_flags_reflection absl_flags_usage absl_flags_usage_internal
#
# 	absl_vlog_config_internal absl_log_internal_message
# 	absl_log_entry absl_log_flags absl_log_globals absl_log_initialize absl_log_internal_check_op absl_log_internal_conditions
# 	absl_log_internal_fnmatch absl_log_internal_format absl_log_internal_globals absl_log_internal_log_sink_set
# 	absl_log_internal_nullguard absl_log_internal_proto absl_log_severity absl_log_sink absl_raw_logging_internal
#
# 	absl_hash absl_low_level_hash
# 	absl_periodic_sampler absl_hashtablez_sampler
#
# 	absl_random_distributions absl_random_internal_distribution_test_util absl_random_internal_platform absl_random_internal_pool_urbg
# 	absl_random_internal_randen absl_random_internal_randen_hwaes absl_random_internal_randen_hwaes_impl absl_random_internal_randen_slow
# 	absl_random_internal_seed_material absl_random_seed_gen_exception absl_random_seed_sequences absl_raw_hash_set
#
# 	absl_scoped_set_env
# 	absl_status absl_statusor
# 	absl_synchronization absl_spinlock_wait absl_kernel_timeout_internal absl_graphcycles_internal
# 	absl_civil_time absl_city absl_time absl_time_zone
# 	absl_leak_check absl_stacktrace absl_examine_stack
# 	absl_symbolize absl_debugging_internal absl_demangle_internal
# 	absl_str_format_internal absl_string_view absl_strings absl_strings_internal
# 	absl_throw_delegate absl_malloc_internal
# 	absl_bad_any_cast_impl absl_bad_optional_access absl_bad_variant_access
# 	absl_failure_signal_handler absl_strerror absl_die_if_null absl_exponential_biased absl_int128 absl_base
# )

add_subdirectory(src_lib_common)
add_subdirectory(src_lib_db)
add_subdirectory(src_lib_protocol)
add_subdirectory(src_app)
