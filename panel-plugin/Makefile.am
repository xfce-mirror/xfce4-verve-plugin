# $Id$

plugindir = $(libdir)/xfce4/panel-plugins

plugin_LTLIBRARIES =							\
	libverveplugin.la

libverveplugin_la_LDFLAGS =						\
	-avoid-version							\
	-export-dynamic							\
	-module

libverveplugin_la_SOURCES =						\
	verve-plugin.c							\
	../verve/verve.c						\
	../verve/verve-db.c						\
	../verve/verve-history.c					\
	../verve/verve-env.c						\
	../verve/verve.h						\
	../verve/verve-db.h						\
	../verve/verve-history.h					\
	../verve/verve-env.h

libverveplugin_la_CFLAGS = 						\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	@LIBEXO_CFLAGS@							\
	@LIBXFCE4PANEL_CFLAGS@						\
	@LIBXFCE4UTIL_CFLAGS@						\
	@LIBPCRE_CFLAGS@						

libverveplugin_la_LIBADD = 						\
	@LIBEXO_LIBS@							\
	@LIBXFCE4PANEL_LIBS@						\
	@LIBXFCE4UTIL_LIBS@						\
	@LIBPCRE_LIBS@

%.desktop.in: %.desktop.in.in
	sed -e "s^@INTERNAL_PLUGIN_PATH@^$(libdir)/xfce4/panel-plugins^"  < $< > $@

desktop_in_in_files = verve.desktop.in.in
desktop_in_files = $(desktop_in_in_files:.desktop.in.in=.desktop.in)

desktopdir = $(datadir)/xfce4/panel-plugins
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST =								\
	$(desktop_in_in_files)

DISTCLEANFILES =							\
	$(desktop_DATA) $(desktop_in_files)		
