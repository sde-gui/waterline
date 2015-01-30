if BUILD_ALSA_PLUGINS

INCLUDES = \
	-I. \
	-I$(top_srcdir)/src \
	$(PACKAGE_CFLAGS) \
	$(G_CAST_CHECKS)

module_LTLIBRARIES = volumealsa.la

moduledir = $(pkglibdir)/plugins

volumealsa_la_SOURCES = volumealsa.c backend.c backend_alsa.c

volumealsa_la_LIBADD = -lasound $(PACKAGE_LIBS)

volumealsa_la_LDFLAGS = \
	-module \
    @WATERLINE_MODULE@

endif
