INCLUDES = \
	-I. \
	-I$(top_srcdir)/src \
	$(PACKAGE_CFLAGS) \
	$(G_CAST_CHECKS)

module_LTLIBRARIES = battery_indicator.la

moduledir = $(pkglibdir)/plugins

battery_indicator_la_SOURCES = \
	battery_indicator.c \
	battery.h \
	battery.c


battery_indicator_la_LIBADD = \
	$(PACKAGE_LIBS)

battery_indicator_la_LDFLAGS = \
	-module \
	@WATERLINE_MODULE@
