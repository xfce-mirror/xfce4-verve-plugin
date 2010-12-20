plugindir = $(libexecdir)/xfce4/panel-plugins

plugin_PROGRAMS =							\
	xfce4-verve-plugin

xfce4_verve_plugin_SOURCES =						\
 	verve-plugin.c							\
	verve.c								\
	verve-history.c							\
	verve-env.c							\
	verve.h								\
	verve-history.h							\
	verve-env.h

xfce4_verve_plugin_CFLAGS = 						\
	-I$(top_builddir)						\
	-I$(top_srcdir)							\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	@LIBEXO_CFLAGS@							\
	@LIBXFCE4PANEL_CFLAGS@						\
	@LIBXFCE4UTIL_CFLAGS@						\
	@LIBXFCEGUI4_CFLAGS@						\
	@LIBPCRE_CFLAGS@						\
	@GLIB_CFLAGS@							\
	@GTHREAD_CFLAGS@						

xfce4_verve_plugin_LDADD = 						\
	@LIBEXO_LIBS@							\
	@LIBXFCE4PANEL_LIBS@						\
	@LIBXFCE4UTIL_LIBS@						\
	@LIBXFCEGUI4_LIBS@						\
	@LIBPCRE_LIBS@							\
	@GLIB_LIBS@							\
	@GTHREAD_LIBS@

if HAVE_DBUS
xfce4_verve_plugin_SOURCES += 						\
	verve-dbus-service-infos.h					\
	verve-dbus-service.h						\
	verve-dbus-service.c						

xfce4_verve_plugin_CFLAGS +=						\
	-DDBUS_API_SUBJECT_TO_CHANGE					\
	$(DBUS_CFLAGS)

xfce4_verve_plugin_LDADD += 						\
	$(DBUS_LIBS)

verve-dbus-service-infos.h: Makefile $(srcdir)/verve-dbus-service-infos.xml 
	dbus-binding-tool --prefix=verve_dbus_service --mode=glib-server $(srcdir)/verve-dbus-service-infos.xml > verve-dbus-service-infos.h

BUILT_SOURCES = 							\
	verve-dbus-service-infos.h
endif

# .desktop file
#
# Some automake trickery here. Because we cannot use $(libexecdir) in the
# automake stage, we'll use sed to get the full path into the .desktop file.
# We also need to let intltool merge the translated fields, so we add an
# additional level of indirection: a <name>.desktop.in.in file.
# 
desktop_in_in_files = xfce4-verve-plugin.desktop.in.in
desktop_in_files = $(desktop_in_in_files:.desktop.in.in=.desktop.in)
desktopdir = $(datadir)/xfce4/panel-plugins
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = 								\
 	$(desktop_in_in_files)						\
	verve-dbus-service-infos.xml
 
DISTCLEANFILES =							\
	$(desktop_DATA) $(desktop_in_files)

if HAVE_DBUS
DISTCLEANFILES +=							\
	verve-dbus-service-infos.h
endif
	
# get full path into .desktop file
%.desktop.in: %.desktop.in.in
	sed -e "s^@PLUGIN_PATH@^$(libexecdir)/xfce4/panel-plugins^" 	\
		$< > $@

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
