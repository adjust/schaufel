AC_INIT(schaufel, 0.5)
AC_CONFIG_SRCDIR([src/modules.c])
#AC_CONFIG_AUX_DIR([build-aux])

AC_LANG(C)
AC_PROG_CC

AX_CHECK_COMPILE_FLAG([-std=c11], [
    CFLAGS+=" -std=c11"
], [
    AC_MSG_ERROR([C compiler cannot compile C11 code])
])

dnl The dlopen() function is in the C library for *BSD and in
dnl libdl on GLIBC-based systems
AC_SEARCH_LIBS([dlopen], [dl dld], [], [
    AC_MSG_ERROR([unable to find the dlopen() function])
])

AC_CHECK_LIB([config], [config_init], [], [
    AC_MSG_ERROR([unable to find the config_init() function])
])

AC_SEARCH_LIBS([pthread_create], [pthread], [
    PTHREAD_LIB=$ac_cv_search_pthread_create
    AC_SUBST([PTHREAD_LIB])
    ], [
    AC_MSG_ERROR([unable to find the pthread_create() function])
])

CFLAGS+=" -Wall -Wextra -pedantic"
CFLAGS+=" -D_POSIX_C_SOURCE=200809L"
CFLAGS+=" -D_BSD_SOURCE"

AC_CONFIG_HEADERS([src/include/build.h])

AC_OUTPUT([Makefile Makefile.global])
