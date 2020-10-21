AC_INIT([iotg-tsn-ref-sw], [1.0])
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
AC_PROG_CC

PKG_CHECK_MODULES([libelf], [libelf])
PKG_CHECK_MODULES([libbpf], [libbpf])
PKG_CHECK_MODULES([libjson], [json-c])
PKG_CHECK_MODULES([open62451], [open62541])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT

