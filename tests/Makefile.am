# $Id$

INCLUDES =								\
	-I$(top_builddir)						\
	-I$(top_srcdir)							\
	-DBINDIR=\"$(bindir)\"						\
	-DDATADIR=\"$(datadir)\"					\
	-DG_LOG_DOMAIN=\"Verve\"					\
	-DLIBDIR=\"$(libdir)\"						\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	$(PLATFORM_CPPFLAGS)

bin_PROGRAMS =								\
	verve-test

#test_built_sources =							\
#	verve-test.c

test_SOURCES =								\
	../verve/verve.c						\
	../verve/verve.h						\
	../verve/verve-db.c						\
	../verve/verve-db.h						\
	../verve/verve-env.c						\
	../verve/verve-env.h						\
	../verve/verve-history.c					\
	../verve/verve-history.h					\
	verve-test.c

test_CFLAGS =								\
	$(LIBXFCE4PANEL_CFLAGS)						\
	$(LIBXFCE4UTIL_CFLAGS						\
	$(LIBEXO_CFLAGS)						\
	$(LIBPCRE_CFLAGS)						\
	$(GLIB_CFLAGS)							\
	$(GTHREAD_CFLAGS)						

test_LDFLAGS =								\
	-no-undefined							\
	$(PLATFORM_LDFLAGS)						\
	$(LIBXFCE4PANEL_LDFLAGS)					\
	$(LIBXFCE4UTIL_LDFLAGS)						\
	$(LIBEXO_LDFLAGS)						\
	$(LIBPCRE_LDFLAGS)						\
	$(GLIB_LDFLAGS)							\
	$(GTHREAD_LDFLAGS)

test_LDADD =								\
	$(LIBXFCE4PANEL_LIBS)						\
	$(LIBXFCE4UTIL_LIBS)						\
	$(LIBEXO_LIBS)							\
	$(LIBPCRE_LIBS)							\
	$(GLIB_LIBS)							\
	$(GTHREAD_LIBS)

clean-local:
	rm -f *.core core core.*

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
