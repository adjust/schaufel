# SYNOPSIS
#
#   AX_PG_CONFIG()
#
# DESCRIPTION
#
#   Populates environmental variables POSTGRESQL_CFLAGS, POSTGRESQL_CPPFLAGS,
#   PG_CONFIG and POSTGRESQL_VERSION variables from pg_config.
#   Errors out if any of the required dependencies are not met.
#   Adapted from AX_LIB_POSTGRESQL.
#
#   Copyright (c) 2008 Mateusz Loskot <mateusz@loskot.net>
#   Copyright (c) 2014 Sree Harsha Totakura <sreeharsha@totakura.in>
#   Copyright (c) 2018 Bastien Roucaries <rouca@debian.org>
#   Copyright (c) 2022 Felix Wischke <felix@zeynix.de>
#
# LICENSE
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

AC_DEFUN([AX_PG_CONFIG],
    [
        AC_PROG_EGREP()
        AC_PROG_AWK()
        test -n "$EGREP" || AC_MSG_ERROR([require egrep])
        test -n "$AWK" || AC_MSG_ERROR([require awk])

        AC_CACHE_CHECK([for the pg_config program], [ac_cv_path_PG_CONFIG],
            [
                AC_PATH_PROGS_FEATURE_CHECK([PG_CONFIG], [pg_config],
                    [
                        pgconfout=`pg_config | $EGREP -c "^VERSION.*PostgreSQL"`
                        test "x$pgconfout" = "x1" && \
                            ac_cv_path_PG_CONFIG=$ac_path_PG_CONFIG

                    ],
                    [AC_MSG_ERROR([could not find pg_config])]
                )
            ]
        )
        AC_SUBST([PG_CONFIG], [$ac_cv_path_PG_CONFIG])

        # populate CFLAG with includedir
        AC_CACHE_CHECK([for required PostgreSQL CFLAGS],[ac_cv_POSTGRESQL_CFLAGS],
            [
                ac_cv_POSTGRESQL_CFLAGS="-I`$PG_CONFIG --includedir`" || \
                    AC_MSG_ERROR([could not execute $PG_CONFIG --includedir])
            ]
        )
        # populate CPPFLAGS with CFLAGS
        AC_CACHE_CHECK([for required PostgreSQL CPPFLAGS],[ac_cv_POSTGRESQL_CPPFLAGS],
            [ac_cv_POSTGRESQL_CPPFLAGS="$ac_cv_POSTGRESQL_CFLAGS"])

        # populate VERSION
        AC_CACHE_CHECK([for PostgreSQL version], [ac_cv_POSTGRESQL_VERSION],
            [
                ac_cv_POSTGRESQL_VERSION=`$PG_CONFIG --version | $AWK '{print @S|@2}' | \
                    $EGREP -o '^[[0-9]]+\.[[0-9]]+'` || \
                    AC_MSG_ERROR([could not determine postgres version])
            ]
        )
        AC_SUBST([POSTGRESQL_VERSION],[$ac_cv_POSTGRESQL_VERSION])
        AC_SUBST([POSTGRESQL_CFLAGS],[$ac_cv_POSTGRESQL_CFLAGS])
        AC_SUBST([POSTGRESQL_CPPFLAGS],[$ac_cv_POSTGRESQL_CPPFLAGS])
    ]
)

