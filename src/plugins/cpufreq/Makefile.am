INCLUDES = \
	-I. \
	-I$(top_srcdir)/src \
	$(PACKAGE_CFLAGS) \
	$(G_CAST_CHECKS)

module_LTLIBRARIES = cpufreq.la

moduledir = $(pkglibdir)/plugins

cpufreq_la_SOURCES = \
	cpufreq.c

cpufreq_la_LIBADD = \
	$(PACKAGE_LIBS)

cpufreq_la_LDFLAGS = \
	-module \
	@WATERLINE_MODULE@
