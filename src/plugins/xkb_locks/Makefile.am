INCLUDES = \
    -I. \
    -I$(top_srcdir)/src \
    $(PACKAGE_CFLAGS) \
    $(G_CAST_CHECKS)

module_LTLIBRARIES = xkb_locks.la

moduledir = $(pkglibdir)/plugins

xkb_locks_la_SOURCES = xkb_locks.c

xkb_locks_la_LIBADD = $(PACKAGE_LIBS)

xkb_locks_la_LDFLAGS = \
    -module \
    @WATERLINE_MODULE@
