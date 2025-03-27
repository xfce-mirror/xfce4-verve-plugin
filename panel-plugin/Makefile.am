plugindir = $(libdir)/xfce4/panel/plugins

plugin_LTLIBRARIES = libverve.la

libverve_la_SOURCES =							\
 	verve-plugin.c							\
	verve.c								\
	verve-history.c							\
	verve-env.c							\
	verve.h								\
	verve-history.h							\
	verve-env.h

libverve_la_CFLAGS =	 						\
	-I$(top_builddir)						\
	-I$(top_srcdir)							\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	@LIBXFCE4PANEL_CFLAGS@						\
	@LIBXFCE4UI_CFLAGS@						\
	@PCRE2_CFLAGS@							\
	@GTHREAD_CFLAGS@						

libverve_la_LIBADD = 							\
	@LIBXFCE4PANEL_LIBS@						\
	@LIBXFCE4UI_LIBS@						\
	@PCRE2_LIBS@							\
	@GTHREAD_LIBS@

libverve_la_LDFLAGS = \
	-avoid-version \
	-module \
	-no-undefined \
	-export-symbols-regex '^xfce_panel_module_(preinit|init|construct)' \
	$(PLATFORM_LDFLAGS)


# .desktop file
#
desktop_in_files = xfce4-verve-plugin.desktop.in
desktopdir = $(datadir)/xfce4/panel/plugins
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
$(desktop_DATA): $(desktop_in_files) Makefile
	$(AM_V_GEN)$(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

EXTRA_DIST = 								\
	$(desktop_in_files)						\
	$(NULL)
 
DISTCLEANFILES = $(desktop_DATA)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
