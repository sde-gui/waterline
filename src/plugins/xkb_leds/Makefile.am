INCLUDES = \
    -I. \
    -I$(top_srcdir)/src \
    $(PACKAGE_CFLAGS) \
    $(G_CAST_CHECKS)

module_LTLIBRARIES = xkb_leds.la

moduledir = $(pkglibdir)/plugins

xkb_leds_la_SOURCES = xkb_leds.c

xkb_leds_la_LIBADD = $(PACKAGE_LIBS)

xkb_leds_la_LDFLAGS = \
    -module \
    @WATERLINE_MODULE@
