# $Id$

@SET_MAKE@

SUBDIRS =								\
	panel-plugin

distclean-local:
	rm -rf *.spec *.cache *~

rpm: dist
	rpmbuild -ta $(PACKAGE)-$(VERSION).tar.gz
	@rm -f $(PACKAGE)-$(VERSION).tar.gz

EXTRA_DIST =								\
	README								\
	THANKS								\
	AUTHORS								\
	COPYING

