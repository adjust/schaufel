set(src
	consumer.c
	dummy.c
	exports.c
	file.c
	hooks.c
	kafka.c
	main.c
	postgres.c
	producer.c
	queue.c
	redis.c
	validator.c

	utils/array.c
	utils/bintree.c
	utils/config.c
	utils/fnv.c
	utils/helper.c
	utils/logger.c
	utils/metadata.c
	utils/postgres.c
	utils/scalloc.c
	utils/strlwr.c

	hooks/dummy.c
	hooks/jsonexport.c
	hooks/xmark.c
)

include(CheckFunctionExists)
check_function_exists(__builtin_bswap64 HAVE__BUILTIN_BSWAP64)
if (HAVE__BUILTIN_BSWAP64)
    add_compile_definitions(HAVE__BUILTIN_BSWAP64)
endif()

check_function_exists(__builtin_bswap32 HAVE__BUILTIN_BSWAP32)
if (HAVE__BUILTIN_BSWAP32)
    add_compile_definitions(HAVE__BUILTIN_BSWAP32)
endif()

include (TestBigEndian)
TEST_BIG_ENDIAN(WORDS_BIGENDIAN)
if (WORDS_BIGENDIAN)
    add_compile_definitions(WORDS_BIGENDIAN)
endif()

add_executable(schaufel ${src})
target_include_directories(schaufel
	PRIVATE
		${schaufel_SOURCE_DIR}/src
		${schaufel_SOURCE_DIR}/src/utils
		${schaufel_SOURCE_DIR}/src/hooks
		${PGSQL_INCLUDEDIR}
		${LIBCONFIG_INCLUDE_DIRS}
		${RDKAFKA_INCLUDE_DIRS}
		${JSONC_INCLUDE_DIRS}
		${HIREDIS_INCLUDE_DIRS}
)
target_link_libraries(schaufel PRIVATE
	${LIBCONFIG_LDFLAGS}
	${HIREDIS_LDFLAGS}
	${JSONC_LDFLAGS}
	${RDKAFKA_LIBRARIES}
	-L${PGSQL_LIBDIR} pq
	Threads::Threads
)
target_compile_options(schaufel PRIVATE
	-Wall -Wextra -pedantic
	-D_XOPEN_SOURCE=700
	-D_SCHAUFEL_VERSION=\"${CMAKE_PROJECT_VERSION}\"
)
install(TARGETS schaufel DESTINATION bin)
