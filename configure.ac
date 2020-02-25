AC_INIT(dpaste, 0.3.3)
AC_CONFIG_AUX_DIR(ac)
AM_INIT_AUTOMAKE([foreign subdir-objects])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CANONICAL_HOST

AC_LANG_PUSH([C++])
AX_CXX_COMPILE_STDCXX(17,[noext],[mandatory])
# explicit c++17 in flags so that compile_commands.json yields it.
CXXFLAGS="${CXXFLAGS} -std=c++17"

AC_ARG_ENABLE([debug], AS_HELP_STRING([--enable-debug], [Build in debug mode, adds stricter warnings, disables optimization]))
AS_IF([test "x$enable_debug" = "xyes"],
      [CXXFLAGS="${CXXFLAGS} -g -Wno-return-type -Wall -Wextra -Wnon-virtual-dtor -O0"],
      [CXXFLAGS="${CXXFLAGS} -O3"])

AC_PROG_CXX
AC_PROG_RANLIB

PKG_CHECK_MODULES([OpenDHT], [opendht >= 1.2])
PKG_CHECK_MODULES([CURLPP], [curlpp])
PKG_CHECK_MODULES([GLIBMM], [glibmm-2.4])

# dpaste (CPP/LD)FLAGS common with different binaries (particularly tests)
AC_SUBST(OpenDHT_LIBS, "${OpenDHT_LIBS} -lpthread")

AM_PATH_GPGME()

AC_CONFIG_FILES([Makefile
                 src/Makefile])

################
#  Unit tests  #
################
AC_ARG_ENABLE([tests], AS_HELP_STRING([--enable-tests], [Enables unit tests compilation]))
AM_CONDITIONAL([DPASTE_TEST], [test "x$enable_tests" = "xyes"])
AM_COND_IF([DPASTE_TEST],
           [
            AC_DEFINE([DPASTE_TEST], [1], [Unit tests])
            AC_CONFIG_FILES([tests/Makefile])
           ])

AC_OUTPUT

# vim: set ts=2 sw=2 tw=120 et :

