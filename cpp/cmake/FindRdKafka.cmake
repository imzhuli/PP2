find_path(LIBRDKAFKA_INCLUDE_DIR librdkafka/rdkafkacpp.h)

set(LIB_NAMES rdkafka rdkafka++)
set(LIBRDKAFKA_LIBRARIES)
 
foreach(lib IN LISTS LIB_NAMES)
    find_library(${lib}_PATH NAMES ${lib})
    if(${lib}_PATH)
        message(STATUS "Found: " ${lib}_PATH)
        list(APPEND LIBRDKAFKA_LIBRARIES ${${lib}_PATH})
    endif()
    mark_as_advanced(${lib}_PATH)
endforeach()


include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(rdkafka::rdkafka
    REQUIRED_VARS LIBRDKAFKA_LIBRARIES LIBRDKAFKA_INCLUDE_DIR
)


if (rdkafka::rdkafka_FOUND)
    set(LIBRDKAFKA_INCLUDE_DIRS ${LIBRDKAFKA_INCLUDE_DIR})
    set(LIBRDKAFKA_LIBRARIES ${LIBRDKAFKA_LIBRARIES})
else()
    set(LIBRDKAFKA_INCLUDE_DIRS)
    set(LIBRDKAFKA_LIBRARIES)
endif()

message(STATUS ${LIBRDKAFKA_LIBRARIES})
mark_as_advanced(LIB_NAMES LIBRDKAFKA_INCLUDE_DIR)


