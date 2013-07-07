## Process this file with automake to produce Makefile.in

DIST_SUBDIRS=$(ALL_PLUGINS_LIST)

if BUILD_PLUGIN_LOADER
DYNAMIC_PLUGINS = \
	$(PLUGINS_LIST)
endif

SUBDIRS = $(DYNAMIC_PLUGINS)

noinst_LIBRARIES = libbuiltin_plugins.a

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/plugins \
	$(PACKAGE_CFLAGS) \
	$(G_CAST_CHECKS)

if ENABLE_MENU_CACHE
#MENU_SOURCES = \
#	menu.c \
#	launchbar.c
endif

PLUGINS_SOURCES = \
	launchbutton.c \
	dclock.c \
	$(MENU_SOURCES) \
	separator.c \
	pager.c \
	space.c \
	tray.c \
	wincmd.c
#	dirmenu.c

libbuiltin_plugins_a_SOURCES = \
	$(PLUGINS_SOURCES)

libbuiltin_plugins_a_LIBADD =

install-exec-hook:
	rm -f $(DESTDIR)$(pkglibdir)/plugins/*.la
	rm -f $(DESTDIR)$(pkglibdir)/plugins/*.a
	rm -f $(DESTDIR)$(pkglibdir)/plugins/*.
