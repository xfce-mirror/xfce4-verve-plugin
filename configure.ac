dnl Copyright (c) 2006 - Jannis Pohlmann <jannis@xfce.org>
dnl Copyright (c) 2010 - Florian Rivoal <frivoal@xfce.org>
dnl Copyright (c) 2015 - Isaac Schemm <isaacschemm@gmail.com>
dnl

XDT_VERSION_INIT([2.0.4], [git])

dnl ***************************
dnl *** Initialize autoconf ***
dnl ***************************
AC_COPYRIGHT([Copyright (c) 2006-2008 Jannis Pohlmann <jannis@xfce.org>
Copyright (c) Florian Rivoal 2010 <frivoal@xfce.org>
Copyright (c) Isaac Schemm 2015 <isaacschemm@gmail.com>
Copyright (c) 2006-2024 The Xfce development team. All rights reserved.])
AC_INIT([xfce4-verve-plugin], [xdt_version], [https://gitlab.xfce.org/panel-plugins/xfce4-verve-plugin], [xfce4-verve-plugin])
AC_REVISION([xdt_version_build])
AC_PREREQ([2.69])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

dnl ***************************
dnl *** Initialize automake ***
dnl ***************************
AM_INIT_AUTOMAKE([1.8 dist-bzip2 no-dist-gzip tar-ustar foreign])
AC_CONFIG_HEADERS([config.h])
AM_MAINTAINER_MODE()

dnl *******************************
dnl *** Check for UNIX variants ***
dnl *******************************
AC_USE_SYSTEM_EXTENSIONS

dnl ********************************
dnl *** Check for basic programs ***
dnl ********************************
AC_PROG_CC()
LT_PATH_LD([])
AC_PROG_INSTALL()
AC_CONFIG_MACRO_DIRS([m4])

dnl **************************
dnl *** Initialize libtool ***
dnl **************************
LT_PREREQ([2.2.6])
LT_INIT([disable-static])

dnl ******************************
dnl *** Check for i18n support ***
dnl ******************************
GETTEXT_PACKAGE="$PACKAGE"
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"], [Name of default gettext domain])
AC_SUBST([GETTEXT_PACKAGE])
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19.8])

dnl ***********************************
dnl *** Check for required packages ***
dnl ***********************************
XDT_CHECK_PACKAGE([GLIB], [glib-2.0], [2.50.0])
XDT_CHECK_PACKAGE([GTHREAD], [gthread-2.0], [2.50.0])
XDT_CHECK_PACKAGE([GTK], [gtk+-3.0], [3.22.0])
XDT_CHECK_PACKAGE([LIBXFCE4PANEL], [libxfce4panel-2.0], [4.16.0])
XDT_CHECK_PACKAGE([LIBXFCE4UI], [libxfce4ui-2], [4.16.0])
XDT_CHECK_PACKAGE([PCRE2], [libpcre2-8], [10.00])

dnl *******************************************************
dnl ** Determine if wordexp is available for expanding   **
dnl ** pathnames entered into the verve input box        **
dnl *******************************************************
AC_CHECK_FUNCS(wordexp)

dnl ***********************************
dnl *** Check for debugging support ***
dnl ***********************************
XDT_FEATURE_DEBUG([xdt_debug_default])

AC_CONFIG_FILES([
Makefile
panel-plugin/Makefile
po/Makefile.in
])
AC_OUTPUT
