INCLUDES = \
	-I. \
	-I$(top_srcdir)/src \
	$(PACKAGE_CFLAGS) \
	$(G_CAST_CHECKS)

module_LTLIBRARIES = taskbar.la

moduledir = $(pkglibdir)/plugins

taskbar_la_SOURCES = \
	taskbar.c

taskbar_la_LIBADD = \
	$(PACKAGE_LIBS)

taskbar_la_LDFLAGS = \
	-module \
	@LXPANELX_MODULE@

EXTRA_DIST = \
	icon.xpm
