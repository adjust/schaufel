AC_INIT(schaufel, 0.5)
AC_LANG(C)
AC_PROG_CC

AM_INIT_AUTOMAKE([foreign -Wall -Werror])

#AC_USE_SYSTEM_EXTENSIONS
#AC_CHECK_FUNCS(getopt)
#AC_CHECK_HEADERS([fcntl.h])
#AC_CHECK_HEADER_STDBOOL

AX_CHECK_COMPILE_FLAG([-std=c11], [
    CFLAGS+=" -std=c11"
], [
    echo "C compiler cannot compile C11 code"
    exit -1
])

dnl The dlopen() function is in the C library for *BSD and in
dnl libdl on GLIBC-based systems
AC_SEARCH_LIBS([dlopen], [dl dld], [], [
    AC_MSG_ERROR([unable to find the dlopen() function])
])

AC_CHECK_LIB([config], [config_init], [], [
    AC_MSG_ERROR([unable to find the config_init() function])
])

AC_CHECK_LIB([pthread], [pthread_create], [], [
    AC_MSG_ERROR([unable to find the pthread_create() function])
])

#TODO: move to the contribs
AC_CHECK_LIB([pq], [PQputCopyData], [], [
    AC_MSG_ERROR([unable to find the PQputCopyData() function])
])

AC_DEFINE_UNQUOTED([VERSION_NUM], [$VERSION], [Numeric version])

CFLAGS+=" -Wall -Wextra -pedantic"
CFLAGS+=" -D_POSIX_C_SOURCE=200809L"
CFLAGS+=" -D_BSD_SOURCE"

AC_CONFIG_HEADERS([src/config.h])

AC_OUTPUT(Makefile.global)
AC_OUTPUT(Makefile)
