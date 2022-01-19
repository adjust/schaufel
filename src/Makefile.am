bin_PROGRAMS = schaufel
schaufel_SOURCES = \
	dummy.c main.c queue.c exports.c hooks.c postgres.c validator.c consumer.c \
	redis.c file.c kafka.c producer.c \
	hooks/dummy.c hooks/jsonexport.c hooks/xmark.c \
	utils/array.c utils/fnv.c utils/metadata.c utils/strlwr.c utils/bintree.c \
	utils/helper.c utils/postgres.c utils/config.c utils/logger.c utils/scalloc.c \
	utils/htable.c

schaufel_LDFLAGS = @LIBS@
