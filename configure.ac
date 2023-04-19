AC_PREREQ([2.71])

AC_INIT([gzdec], [1.0.0], [rodrigovalente1996@gmail.com])
AM_INIT_AUTOMAKE([foreign -Wall -Werror])

AM_PROG_AR

LT_INIT
AC_PROG_CC

AC_ENABLE_SHARED

PKG_CHECK_MODULES([ZLIB], [zlib],[],[AC_MSG_ERROR([cannot build plugin, failed to find zlib (compilation requirement)])])
PKG_CHECK_MODULES([GST], [gstreamer-1.0, gstreamer-base-1.0],[],[AC_MSG_ERROR([cannot build plugin, failed to find gstreamer-1.0 (compilation requirement)])])

AC_CONFIG_FILES([Makefile src/Makefile])

AC_DEFINE(PLUGIN_DESCRIPTION,"GzDec a gzip & bzip decompresser gstreamer plugin")
AC_DEFINE(GST_PACKAGE_NAME,"GzDec gstreamer plugin")
AC_DEFINE(GST_PACKAGE_ORIGIN,"github.com/rodrigovb96/gzdec")
AC_DEFINE(GST_LICENSE,"LGPL")

AC_OUTPUT
