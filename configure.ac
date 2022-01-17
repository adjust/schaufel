AC_PREREQ([2.60])
AC_INIT([schaufel], [0.11], [ops@adjust.com], [https://github.com/adjust/schaufel])
AM_INIT_AUTOMAKE([-Wall subdir-objects])

############################################################
# Check for C compiler
############################################################
AC_PROG_CC

############################################################
# build config.h
############################################################
AC_CONFIG_HEADERS([config.h])
# todo: remove. hash_r functions require gnu source atm
AC_DEFINE([_GNU_SOURCE], 1, [Enable _GNU_SOURCE])
# Features for questionable systems
AC_HEADER_STDBOOL
AC_C_INLINE
AC_C_CONST
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

############################################################
# Check libpthread
############################################################
AC_CHECK_HEADERS([pthread.h],,AC_MSG_ERROR([pthread.h is required!]))
# todo: some systems integrate libpthread into libc
LIBS="$LIBS -lpthread"
# pthread_cleanup_{push,pop} are commonly defined as macros.
AC_MSG_CHECKING([pthread_cleanup_{push,pop} compatibility])
AC_LINK_IFELSE([
    AC_LANG_PROGRAM(
        [#include <pthread.h>],
        [
            pthread_t th; pthread_join(th, 0);
            pthread_cleanup_push(0, 0);
            pthread_create(0,0,0,0);
            pthread_cleanup_pop(0);
        ]
    )],
    [AC_MSG_RESULT([yes])],
    [
        AC_MSG_RESULT([no])
        AC_MSG_ERROR([pthread_cleanup_{pop,push} is required!])
    ]
)
safeLIBS="$LIBS"
AC_CHECK_LIB([pthread],pthread_cancel,,AC_MSG_ERROR([pthread_cancel not found!]))
AC_CHECK_LIB([pthread],pthread_cond_broadcast,,AC_MSG_ERROR([pthread_cond_broadcast not found!]))
AC_CHECK_LIB([pthread],pthread_cond_destroy,,AC_MSG_ERROR([pthread_cond_destroy not found!]))
AC_CHECK_LIB([pthread],pthread_cond_init,,AC_MSG_ERROR([pthread_cond_init not found!]))
AC_CHECK_LIB([pthread],pthread_cond_timedwait,,AC_MSG_ERROR([pthread_cond_timedwait not found!]))
AC_CHECK_LIB([pthread],pthread_cond_wait,,AC_MSG_ERROR([pthread_cond_wait not found!]))
AC_CHECK_LIB([pthread],pthread_create,,AC_MSG_ERROR([pthread_create not found!]))
AC_CHECK_LIB([pthread],pthread_join,,AC_MSG_ERROR([pthread_join not found!]))
AC_CHECK_LIB([pthread],pthread_mutex_destroy,,AC_MSG_ERROR([pthread_mutex_destroy not found!]))
AC_CHECK_LIB([pthread],pthread_mutex_init,,AC_MSG_ERROR([pthread_mutex_init not found!]))
AC_CHECK_LIB([pthread],pthread_mutex_lock,,AC_MSG_ERROR([pthread_mutex_lock not found!]))
AC_CHECK_LIB([pthread],pthread_mutex_unlock,,AC_MSG_ERROR([pthread_mutex_unlock not found!]))
AC_CHECK_LIB([pthread],pthread_testcancel,,AC_MSG_ERROR([pthread_testcancel not found!]))
LIBS="$safeLIBS"

############################################################
# Check libconfig
############################################################
AC_CHECK_HEADERS([libconfig.h],,AC_MSG_ERROR([libconfig.h is required!]))
AC_CHECK_LIB([config],config_init,,AC_MSG_ERROR([libconfig is required!]))

AC_MSG_CHECKING([libconfig version >= 1.7])
AC_RUN_IFELSE([
    AC_LANG_PROGRAM(
        [#include <libconfig.h>],
        [
            #if LIBCONFIG_VER_MAJOR < 1
            return 1;
            #endif
            #if (LIBCONFIG_VER_MAJOR == 1 && LIBCONFIG_VER_MINOR < 7)
            return 1;
            #endif
        ]
    )],
    [AC_MSG_RESULT([yes])],
    [
        AC_MSG_RESULT([no])
        AC_MSG_ERROR([libconfig needs to be at least version 1.7!])
    ]
)

############################################################
# Check librdkafka
############################################################
AC_CHECK_HEADERS([librdkafka/rdkafka.h],,AC_MSG_ERROR([librdfkafka.h is required!]))
LIBS="$LIBS -lrdkafka"

AC_MSG_CHECKING([librdkafka version >= 1.6])
AC_RUN_IFELSE([
    AC_LANG_PROGRAM(
        [#include <librdkafka/rdkafka.h>],
        [
            if (rd_kafka_version() < 0x010600ff)
                return 1;
        ]
    )],
    [AC_MSG_RESULT([yes])],
    [
        AC_MSG_RESULT([no])
        AC_MSG_ERROR([librdkafka needs to be at least version 1.6!])
    ]
)

############################################################
# Check libhiredis
############################################################
AC_CHECK_HEADERS([hiredis/hiredis.h],,AC_MSG_ERROR([hiredis.h is required!]))
AC_CHECK_LIB([hiredis],redisConnect,,AC_MSG_ERROR([libhiredis is required!]))

AC_MSG_CHECKING([libhiredis version >= 0.14])
AC_RUN_IFELSE([
    AC_LANG_PROGRAM(
        [#include <hiredis/hiredis.h>],
        [
            #if (HIREDIS_MAJOR == 0 && HIREDIS_MINOR < 14)
            return 1;
            #endif
        ]
    )],
    [AC_MSG_RESULT([yes])],
    [
        AC_MSG_RESULT([no])
        AC_MSG_ERROR([libhiredis needs to be at least version 0.14!])
    ]
)

############################################################
# Check libpq
############################################################
AC_CHECK_HEADERS([libpq-fe.h],,AC_MSG_ERROR([libpq-fe.h is required!]))
LIBS="$LIBS -lpq"

AC_MSG_CHECKING([lipq-fe >= 9.6])
AC_RUN_IFELSE([
    AC_LANG_PROGRAM(
        [#include <libpq-fe.h>],
        [
            if (PQlibVersion() < 90600)
                return 1;
        ]
    )],
    [AC_MSG_RESULT([yes])],
    [
        AC_MSG_RESULT([no])
        AC_MSG_ERROR([postgres/libpq needs to be at least version 9.6!])
    ]
)

############################################################
# Check bswap functions (endianness portability)
############################################################
AC_C_BIGENDIAN() # WORDS_BIGENDIAN
AX_GCC_BUILTIN(__builtin_bswap32)
AX_GCC_BUILTIN(__builtin_bswap64)

############################################################
AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
