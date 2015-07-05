INCLUDES = \
	-I. \
	-I$(top_srcdir)/src \
	$(PACKAGE_CFLAGS) \
	$(G_CAST_CHECKS)

module_LTLIBRARIES = show_desktop.la

moduledir = $(pkglibdir)/plugins

show_desktop_la_SOURCES = \
	show_desktop.c

show_desktop_la_LIBADD = \
	$(PACKAGE_LIBS)

show_desktop_la_LDFLAGS = \
	-module \
	@WATERLINE_MODULE@
