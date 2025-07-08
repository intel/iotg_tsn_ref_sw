AC_INIT([iotg-tsn-ref-sw], [1.0])
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
AC_PROG_CC

# Check for XDP header file
AC_CHECK_HEADERS([linux/if_xdp.h], [HAVE_XDP=yes], [HAVE_XDP=no])

HAVE_XDPTBS=no
if test "x${HAVE_XDP}" = "xno"; then
  AC_MSG_WARN([if_xdp.h is not found in the system. AF_XDP is considered as un-supported.])
  AC_MSG_NOTICE([Opcua-server will not be compiled because AF_XDP is mandatory for it. Only txrx-tsn and tsq will be compiled.])
else
  # Check if XDP+TBS Intel specific kernel implementation is available (the extra txtime var in xdp_desc)
  AC_CHECK_MEMBER([struct xdp_desc.txtime],[HAVE_XDPTBS=yes],
                [AC_MSG_WARN([txtime is not found in if_xdp.h:xdp_desc. Intel XDP+TBS is not supported!])],
                [[#include "linux/if_xdp.h"]])
fi

# We can only enable xdp if we support XDP in the system (if_xdp.h exists)
AC_ARG_ENABLE([xdp],
              AS_HELP_STRING([--enable-xdp], [enable xdp[default=yes]]))
AC_MSG_CHECKING([whether we choose to enable socket xdp])
AS_IF([test "x${enable_xdp}" = "xyes"], AC_MSG_RESULT([yes]), AC_MSG_RESULT([no]))

AS_IF([test "x${enable_xdp}" = "xyes"],
  [AS_IF([test "x${HAVE_XDP}" = "xyes"], [use_xdp=yes], [use_xdp=no])],
  [use_xdp=no])

AC_MSG_CHECKING([whether the system supports xdp socket and we are enabling xdp socket usage])
AS_IF([test "x${use_xdp}" = "xyes"], AC_MSG_RESULT([yes]), AC_MSG_RESULT([no]))
AM_CONDITIONAL([WITHXDP], [test "x${use_xdp}" = "xyes"])

# We can only enable xdp_tbs if we support XDP_tbs in the system (if_xdp.h txtime var exists)
AC_ARG_ENABLE(xdptbs,[AS_HELP_STRING([--enable-xdptbs], [enable xdptbs])])
AC_MSG_CHECKING([whether we choose to enable xdp+tbs feature])
AS_IF([test "x${enable_xdptbs}" = "xyes" ], AC_MSG_RESULT([yes]), AC_MSG_RESULT([no]))

AS_IF([test "x${enable_xdptbs}" = "xyes"],
  [AS_IF([test "x${use_xdp}" = "xyes"],
      [AS_IF([test "x${HAVE_XDPTBS}" = "xyes"],[use_xdptbs=yes],[use_xdptbs=no])],
      [use_xdptbs=no])],
  [use_xdptbs=no])

AC_MSG_CHECKING([whether the system supports xdp+tbs and we are enabling xdp+tbs feature])
AS_IF([test "x${use_xdptbs}" = "xyes" ], AC_MSG_RESULT([yes]), AC_MSG_RESULT([no]))
AM_CONDITIONAL([WITHXDPTBS], [test "x${use_xdptbs}" = "xyes"])

if test "x${use_xdp}" = "xyes"; then
  if test "x${use_xdptbs}" = "xyes"; then
    ENABLEXDP_CPPFLAGS="-DWITH_XDP -DWITH_XDPTBS"
  else
    ENABLEXDP_CPPFLAGS="-DWITH_XDP"
  fi
else
  ENABLEXDP_CPPFLAGS=""
fi

AC_SUBST(ENABLEXDP_CPPFLAGS)

#TODO check using AC_SEARCH_LIBS instead of PKG_CHECK_MODULES in the next iteration.
PKG_CHECK_MODULES([libelf], [libelf],,[AC_MSG_WARN([libelf.pc is not found in the system. Pls ensure the lib is installed anyway!!])])
PKG_CHECK_MODULES([libbpf], [libbpf],,[AC_MSG_WARN([libbpf.pc is not found in the system. Pls ensure the lib is installed anyway!!])])
PKG_CHECK_MODULES([libjson], [json-c],,[AC_MSG_WARN([json-c.pc is not found in the system. Pls ensure the lib is installed anyway!!])])

no_open62541_iotg_fork=0

PKG_CHECK_MODULES([open62451],
    [open62541-iotg],
    [AC_DEFINE([HAVE_OPEN62541_IOTG], [1], [Using open62541 using iotg fork])],
    [no_open62541_iotg_fork=1])

if test "x${no_open62541_iotg_fork}" = "x1"; then
  AC_MSG_WARN([open62541-iotg.pc is not found in the system, libopen62541-iotg is considered as uninstalled. Opcua-based test(opcua-server) will NOT be compiled])
  AC_MSG_NOTICE([For libopen62541-iotg manual installation, refer to DEPENDENCIES.md])
fi

AM_CONDITIONAL([NO_OPEN62541_IOTG_LIB], [test "x$no_open62541_iotg_fork" = "x1"])
# Support for af_xdp (if_xdp.h must be in) is mandatory for opcua-server
AM_CONDITIONAL([WITH_OPCUA], [test "x${no_open62541_iotg_fork}" = "x0" && test "x${HAVE_XDP}" = "xyes"])

if test "x${no_open62541_iotg_fork}" = "x0"; then
  if test "x${HAVE_XDP}" = "xno"; then
    AC_MSG_WARN([Without XDP support, opcua-server app will NOT be compiled (for now)])
  fi
fi

# FUTURE implementation :
# (when our opcua-server code can finally use the vanilla upstream version)
# Check if libopen62541-iotg fork is available thru related pc file. Otherwise will use default libopen62541.
# If no opcua lib is found, opcua-server code will not be compiled.
#no_opcua_lib=0
#
#PKG_CHECK_MODULES([open62451],
#    [open62541-iotg],
#    [AC_DEFINE([HAVE_OPEN62541_IOTG], [1], [Using open62541 using iotg fork])],
#    [PKG_CHECK_MODULES([open62541],
#        [open62541],
#        [AC_DEFINE([HAVE_OPEN62541_VANILLA], [1], [Using open62541 using upstreamed version])],
#        [no_opcua_lib=1])])
#
#if test "x${no_opcua_lib}" = "x1"; then
#  AC_MSG_WARN([Both libopen62541 and libopen62541 iot forked are not available in the system. opcua-based test will NOT be compiled.])
#fi
#
#AM_CONDITIONAL([NO_OPCUA_LIB], [test "x$no_opcua" = "x1"])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
