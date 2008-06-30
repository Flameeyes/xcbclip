AC_PREREQ(2.62)

AC_INIT([xcbclip], [1.0])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE(dist-bzip2 no-dist-gzip foreign)

AC_PROG_CC_C99
if test "x$ac_cv_prog_cc_c99" = "xno"; then
   AC_MSG_ERROR([no C99 compiler found, $PACKAGE_NAME requires a C99 compiler.])
fi

PKG_CHECK_MODULES([XCB], [xcb])

AC_CONFIG_FILES([Makefile])

AC_OUTPUT
