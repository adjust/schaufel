check_PROGRAMS = dummy_consumer_test jsonexports_test parse_hostinfo array \
		dummy_producer_test logger_test queue_test bintree_test \
		file_consumer_test logparse_test strlwr_test config_merge_test \
		fnv_test metadata_test config_test hooks_test parse_connstring
TESTS = $(check_PROGRAMS)

common_sources = $(top_builddir)/src/utils/config.c $(top_builddir)/src/queue.c $(top_builddir)/src/consumer.c $(top_builddir)/src/producer.c $(top_builddir)/src/hooks.c $(top_builddir)/src/validator.c $(top_builddir)/src/utils/logger.c $(top_builddir)/src/utils/scalloc.c $(top_builddir)/src/hooks/dummy.c $(top_builddir)/src/hooks/xmark.c $(top_builddir)/src/hooks/jsonexport.c $(top_builddir)/src/utils/metadata.c $(top_builddir)/src/utils/fnv.c $(top_builddir)/src/utils/bintree.c $(top_builddir)/src/file.c $(top_builddir)/src/exports.c $(top_builddir)/src/postgres.c $(top_builddir)/src/redis.c $(top_builddir)/src/kafka.c $(top_builddir)/src/utils/helper.c $(top_builddir)/src/utils/array.c $(top_builddir)/src/utils/postgres.c $(top_builddir)/src/dummy.c $(top_builddir)/src/utils/strlwr.c


dummy_consumer_test_SOURCES = $(common_sources) dummy_consumer_test.c
dummy_producer_test_SOURCES = $(common_sources) jsonexports_test.c
file_consumer_test_SOURCES = $(common_sources) file_consumer_test.c
jsonexports_test_SOURCES = $(common_sources) jsonexports_test.c
logger_test_SOURCES = $(common_sources) logger_test.c
logparse_test_SOURCES = $(common_sources) logparse_test.c
parse_hostinfo_SOURCES = $(common_sources) parse_hostinfo.c
array_SOURCES = $(common_sources) array.c
queue_test_SOURCES = $(common_sources) queue_test.c
bintree_test_SOURCES = $(common_sources) bintree_test.c
config_test_SOURCES = $(common_sources) config_test.c
config_merge_test_SOURCES = $(common_sources) config_merge_test.c
strlwr_test_SOURCES = $(common_sources) strlwr_test.c
fnv_test_SOURCES = $(common_sources) fnv_test.c
metadata_test_SOURCES = $(common_sources) metadata_test.c
hooks_test_SOURCES = $(common_sources) hooks_test.c
parse_connstring_SOURCES = $(common_sources) parse_connstring.c
