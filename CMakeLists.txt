cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(schaufel VERSION 0.10 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_COLOR_MAKEFILE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (NOT CMAKE_BUILD_TYPE AND
    CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")

# we depend on pg_config for libpq location
find_program(PG_CONFIG pg_config)
if(NOT PG_CONFIG)
	message(FATAL_ERROR "Unable to find 'pg_config'")
endif(NOT PG_CONFIG)

# prepare libpq for linking
exec_program(${PG_CONFIG} ARGS --includedir OUTPUT_VARIABLE PGSQL_INCLUDEDIR)
exec_program(${PG_CONFIG} ARGS --libdir OUTPUT_VARIABLE PGSQL_LIBDIR)
add_library(pq_lib SHARED IMPORTED)
set_target_properties(pq_lib PROPERTIES
	IMPORTED_LOCATION "${PGSQL_LIBDIR}/libpq.so"
	INTERFACE_INCLUDE_DIRECTORIES "${PGSQL_INCLUDEDIR}"
)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBCONFIG REQUIRED libconfig)
pkg_check_modules(JSONC REQUIRED json-c)
pkg_check_modules(HIREDIS REQUIRED hiredis)

# prepare rdkafka library for linking
find_path(RDKAFKA_INCLUDE_DIRS librdkafka/rdkafka.h)
find_library(RDKAFKA_LIBRARY_PATH rdkafka REQUIRED)

add_library(kafka SHARED IMPORTED)
set_target_properties(kafka PROPERTIES
	IMPORTED_LOCATION "${RDKAFKA_LIBRARY_PATH}"
	INTERFACE_INCLUDE_DIRECTORIES "${RDKAFKA_INCLUDE_DIRS}"
)

add_subdirectory (src)
add_subdirectory (man)

enable_testing()
add_subdirectory (t)
