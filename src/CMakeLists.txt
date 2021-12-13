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
)
target_compile_options(schaufel PRIVATE
	-Wall -Wextra -pedantic
	-D_XOPEN_SOURCE=700
	-D_SCHAUFEL_VERSION=\"${CMAKE_PROJECT_VERSION}\"
)
