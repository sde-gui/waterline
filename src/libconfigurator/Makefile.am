INCLUDES = \
	-I. \
	-I$(top_srcdir)/src \
	$(PACKAGE_CFLAGS) \
	$(G_CAST_CHECKS)

module_LTLIBRARIES = libconfigurator.la

moduledir = $(pkglibdir)/internals

libconfigurator_la_SOURCES = \
	configurator.c \
	configurator_plugin_list.c


libconfigurator_la_LIBADD = \
	$(PACKAGE_LIBS)

libconfigurator_la_LDFLAGS = \
	-module \
	@WATERLINE_INTERNAL_MODULE@
