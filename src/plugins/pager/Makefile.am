INCLUDES = \
	-I. \
	-I$(top_srcdir)/src \
	$(PACKAGE_CFLAGS) \
	$(G_CAST_CHECKS)

module_LTLIBRARIES = pager.la

moduledir = $(pkglibdir)/plugins

pager_la_SOURCES = \
	pager.c

pager_la_LIBADD = \
	$(PACKAGE_LIBS)

pager_la_LDFLAGS = \
	-module \
	@WATERLINE_MODULE@

