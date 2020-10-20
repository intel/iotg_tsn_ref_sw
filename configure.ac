AC_INIT([iotg-tsn-ref-sw], [1.0])
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
AC_PROG_CC

AC_CHECK_LIB([elf], [elf_begin],,
	[AC_MSG_ERROR([*** libelf not found on your system])])

AC_CHECK_LIB([bpf], [xsk_socket__create],,
	[AC_MSG_ERROR([*** libbpf not found on your system])])

AC_CHECK_LIB([json-c], [json_object_get],,
	[AC_MSG_ERROR([*** libjson-c not found on your system])])

#TODO: check for open62541 lib
#AC_CHECK_LIB([open62541], [UA_Server_run],,
#	[AC_MSG_ERROR([*** libopen62541 not found on your system])])

AC_ARG_WITH([open62541-headers],
	[AS_HELP_STRING([--with-open62541-headers],
	[specify directory for open62541 headers])],
	[],
	[])
AS_IF([[ test x"$withval" != x ]],
	[open62541_headers="-I${withval}"],
	[open62541_headers=""])
AC_SUBST([OPEN62541_HEADERS], [${open62541_headers}])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([ Makefile ])
AC_OUTPUT

