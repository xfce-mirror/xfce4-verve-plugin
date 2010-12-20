if HAVE_DBUS

bin_PROGRAMS = verve-focus

verve_focus_SOURCES = 							\
	verve-focus.c

verve_focus_CFLAGS = 							\
	-DDBUS_API_SUBJECT_TO_CHANGE					\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	@DBUS_CFLAGS@							\
	@GLIB_CFLAGS@							\
	@LIBXFCE4UTIL_CFLAGS@						

verve_focus_LDADD = 							\
	@DBUS_LIBS@							\
	@GLIB_LIBS@							\
	@LIBXFCE4UTIL_LIBS@

endif

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
