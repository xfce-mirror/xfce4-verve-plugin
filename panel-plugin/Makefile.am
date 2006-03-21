# $Id$

plugindir = $(libexecdir)/xfce4/panel-plugins

plugin_PROGRAMS =							\
	xfce4-verve-plugin

xfce4_verve_plugin_SOURCES =						\
 	verve-plugin.c							\
	../verve/verve.c						\
	../verve/verve-history.c					\
	../verve/verve-env.c						\
	../verve/verve.h						\
	../verve/verve-history.h					\
	../verve/verve-env.h

xfce4_verve_plugin_CFLAGS = 						\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	@LIBEXO_CFLAGS@							\
	@LIBXFCE4PANEL_CFLAGS@						\
	@LIBXFCE4UTIL_CFLAGS@						\
	@LIBPCRE_CFLAGS@						

xfce4_verve_plugin_LDFLAGS = 						\
	@LIBEXO_LIBS@							\
	@LIBXFCE4PANEL_LIBS@						\
	@LIBXFCE4UTIL_LIBS@						\
	@LIBPCRE_LIBS@

# .desktop file
#
# Some automake trickery here. Because we cannot use $(libexecdir) in the
# automake stage, we'll use sed to get the full path into the .desktop file.
# We also need to let intltool merge the translated fields, so we add an
# additional level of indirection: a <name>.desktop.in.in file.
# 
desktop_in_in_files = verve.desktop.in.in
desktop_in_files = $(desktop_in_in_files:.desktop.in.in=.desktop.in)

desktopdir = $(datadir)/xfce4/panel-plugins
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@
 
EXTRA_DIST = 								\
 	$(desktop_in_in_files)
 
DISTCLEANFILES =							\
	$(desktop_DATA) $(desktop_in_files)
	
# get full path into .desktop file
%.desktop.in: %.desktop.in.in
	sed -e "s^@PLUGIN_PATH@^$(libexecdir)/xfce4/panel-plugins^" \
		$< > $@
