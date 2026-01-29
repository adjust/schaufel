AC_PREREQ([2.62])
AC_CONFIG_MACRO_DIR(m4)
AC_INIT([schaufel], [0.12.2], [ops@adjust.com], [https://github.com/adjust/schaufel])
AM_INIT_AUTOMAKE([subdir-objects])

############################################################
# Handle host specific features
############################################################
AC_CANONICAL_HOST
case $host_os in
*bsd*)
    # pkg installs into /usr/local. Therefore a warning
    # happens at each compile step because autotools appends
    # -L/usr/local/lib to each command, not only at linking
    AX_APPEND_FLAG("-Wno-unused-command-line-argument",CFLAGS)
    # implicitly add /usr/include, should config.guess not
    # do the right thing
    AX_APPEND_FLAG("-I/usr/local/include",CFLAGS)
    AX_APPEND_FLAG("-L/usr/local/lib",LDFLAGS)
    ;;
esac

############################################################
# Check for C compiler
############################################################
AC_USE_SYSTEM_EXTENSIONS()
AC_PROG_CC
AX_CHECK_COMPILE_FLAG([-std=c11],,
    AC_MSG_ERROR([Schaufel requires C11 (compiles with C99 but unsupported)])
)
AX_APPEND_FLAG("-std=c11", CFLAGS)
AX_CHECK_COMPILE_FLAG([-Wall],AX_APPEND_FLAG("-Wall", CFLAGS),)
AX_CHECK_COMPILE_FLAG([-Wextra],AX_APPEND_FLAG("-Wextra", CFLAGS),)
AX_CHECK_COMPILE_FLAG([-Wpedantic],AX_APPEND_FLAG("-Wpedantic", CFLAGS),)

############################################################
# build config.h
############################################################
AC_CONFIG_HEADERS([src/schaufel_config.h])
# Features for questionable systems
AC_HEADER_STDBOOL
AC_C_INLINE
AC_C_CONST
AC_C_VOLATILE       # not having volatile will possibly drop messages
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

############################################################
# Check required system headers
############################################################
AC_CHECK_HEADERS([assert.h],,AC_MSG_ERROR([pthread.h is required!]))
AC_CHECK_HEADERS([ctype.h],,AC_MSG_ERROR([ctype.h is required!]))
AC_CHECK_HEADERS([errno.h],,AC_MSG_ERROR([errno.h is required!]))
AC_CHECK_HEADERS([fcntl.h],,AC_MSG_ERROR([fcntl.h is required!]))
AC_CHECK_HEADERS([regex.h],,AC_MSG_ERROR([regex.h is required!]))
AC_CHECK_HEADERS([search.h],,AC_MSG_ERROR([search.h is required!]))
AC_CHECK_HEADERS([signal.h],,AC_MSG_ERROR([signal.h is required!]))
AC_CHECK_HEADERS([stdarg.h],,AC_MSG_ERROR([stdarg.h is required!]))
AC_CHECK_HEADERS([stdatomic.h],,AC_MSG_ERROR([stdatomic.h is required!]))
AC_CHECK_HEADERS([stdbool.h],,AC_MSG_ERROR([stdbool.h is required!]))
AC_CHECK_HEADERS([stddef.h],,AC_MSG_ERROR([stddef.h is required!]))
#Todo: move all stdint.h includes to schaufel.h
AC_CHECK_HEADERS([stdint.h],,AC_MSG_ERROR([stdint.h is required!]))
AC_CHECK_HEADERS([stdlib.h],,AC_MSG_ERROR([stdlib.h is required!]))
AC_CHECK_HEADERS([string.h],,AC_MSG_ERROR([string.h is required!]))
AC_CHECK_HEADERS([syslog.h],,AC_MSG_ERROR([syslog.h is required!]))
AC_CHECK_HEADERS([time.h],,AC_MSG_ERROR([time.h is required!]))
AC_CHECK_HEADERS([unistd.h],,AC_MSG_ERROR([unistd.h is required!]))

############################################################
# Check required C functionality
############################################################
# asan replaces malloc/realloc, making tests fail
case "${CFLAGS}" in
    *-fsanitize=address*)
        AC_MSG_NOTICE([asan compilation detected. Skipping malloc/realloc test!])
        ;;
    *)
        AC_FUNC_MALLOC #todo: replace malloc with CALLOC everywhere
        AC_FUNC_REALLOC
        ;;
esac
AC_CHECK_FUNC(strdup,,AC_MSG_ERROR([can't find strdup!]))
AC_CHECK_FUNC(getline,,AC_MSG_ERROR([can't find getline!]))

# c11 atomics support
AC_MSG_CHECKING([c11 atomics support])
AC_LINK_IFELSE([
    AC_LANG_PROGRAM(
        [#include <stdatomic.h>
         #include <stdbool.h>],
        [
            #ifdef __STDC_NO_ATOMICS__
                #error c11 atomics are not supported
            #else
            static volatile atomic_bool produce_state = ATOMIC_VAR_INIT(true);
            return 0;
            #endif
        ]
    )],
    [AC_MSG_RESULT([yes])],
    [
        AC_MSG_RESULT([no])
        AC_MSG_ERROR([C compiler doesn't support c11 atomics!])
    ]
)

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

AC_MSG_CHECKING([libconfig version >= 1.5])
AC_RUN_IFELSE([
    AC_LANG_PROGRAM(
        [#include <libconfig.h>],
        [
            #if LIBCONFIG_VER_MAJOR < 1
            return 1;
            #endif
            #if (LIBCONFIG_VER_MAJOR == 1 && LIBCONFIG_VER_MINOR < 5)
            return 1;
            #endif
        ]
    )],
    [AC_MSG_RESULT([yes])],
    [
        AC_MSG_RESULT([no])
        AC_MSG_ERROR([libconfig needs to be at least version 1.5!])
    ]
)

############################################################
# Check libjson-c
############################################################
AC_CHECK_HEADERS([json-c/json.h],,AC_MSG_ERROR([json.h is required!]))
LIBS="$LIBS -ljson-c"

AC_MSG_CHECKING([libjson-c >= 0.13])
AC_RUN_IFELSE([
    AC_LANG_PROGRAM(
        [#include <json-c/json.h>],
        [
            if (json_c_version_num() < (0<<16 | 13<<8))
                return 1;
        ]
    )],
    [AC_MSG_RESULT([yes])],
    [
        AC_MSG_RESULT([no])
        AC_MSG_ERROR([json-c needs to be at least version 0.13!])
    ]
)

############################################################
# Check librdkafka
############################################################
AC_CHECK_HEADERS([librdkafka/rdkafka.h],,AC_MSG_ERROR([librdfkafka.h is required!]))
LIBS="$LIBS -lrdkafka"

AC_MSG_CHECKING([librdkafka version >= 1.2.1])
AC_RUN_IFELSE([
    AC_LANG_PROGRAM(
        [#include <librdkafka/rdkafka.h>],
        [
            if (rd_kafka_version() < 0x010201ff)
                return 1;
        ]
    )],
    [AC_MSG_RESULT([yes])],
    [
        AC_MSG_RESULT([no])
        AC_MSG_ERROR([librdkafka needs to be at least version 1.2.1!])
    ]
)

############################################################
# Check libhiredis
############################################################
AC_CHECK_HEADERS([hiredis/hiredis.h],,AC_MSG_ERROR([hiredis.h is required!]))
AC_CHECK_LIB([hiredis],redisConnect,,AC_MSG_ERROR([libhiredis is required!]))

AC_MSG_CHECKING([libhiredis version >= 0.13])
AC_RUN_IFELSE([
    AC_LANG_PROGRAM(
        [#include <hiredis/hiredis.h>],
        [
            #if (HIREDIS_MAJOR == 0 && HIREDIS_MINOR < 13)
            return 1;
            #endif
        ]
    )],
    [AC_MSG_RESULT([yes])],
    [
        AC_MSG_RESULT([no])
        AC_MSG_ERROR([libhiredis needs to be at least version 0.13!])
    ]
)

############################################################
# Check libpq
############################################################
# get postgresqls server includedir from pg_config
AX_PG_CONFIG()
CFLAGS="$CFLAGS $POSTGRESQL_CFLAGS"
# stupid hack for the preprocessor header check (uses CPPFLAGS)
CPPFLAGS_SAFE="$CPPFLAGS"
CPPFLAGS="$CFLAGS"
AC_CHECK_HEADERS([libpq-fe.h],,AC_MSG_ERROR([libpq-fe.h is required!]))
CPPFLAGS="$CPPFLAGS_SAFE"
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
AC_CONFIG_FILES([Makefile src/Makefile t/Makefile])
AC_OUTPUT
