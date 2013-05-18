
INCLUDES = \
	-I. \
	-I$(top_srcdir)/src \
	$(PACKAGE_CFLAGS) \
	$(G_CAST_CHECKS)

module_LTLIBRARIES = notification_area.la

moduledir = $(pkglibdir)/plugins

notification_area_la_SOURCES = \
	fixedtip.h \
	fixedtip.c \
	na-marshal.c \
	na-marshal.h \
	na-tray.c \
	na-tray.h \
	na-tray-child.c \
	na-tray-child.h \
	na-tray-manager.c \
	na-tray-manager.h \
	main.c


notification_area_la_LIBADD = \
	$(PACKAGE_LIBS)

notification_area_la_LDFLAGS = \
	-module \
	@LXPANELX_MODULE@


#noinst_PROGRAMS = testtray

#testtray_SOURCES = testtray.c
#testtray_LDADD =			\
#	libtray.la \
#	$(X_LIBS)			\
#	$(NOTIFICATION_AREA_LIBS)

na-marshal.h: na-marshal.list $(GLIB_GENMARSHAL)
	$(AM_V_GEN)$(GLIB_GENMARSHAL) $< --header --prefix=_na_marshal > $@

na-marshal.c: na-marshal.list $(GLIB_GENMARSHAL)
	$(AM_V_GEN)echo "#include \"na-marshal.h\"" > $@.tmp && \
	$(GLIB_GENMARSHAL) $< --body --prefix=_na_marshal >> $@.tmp && \
	mv $@.tmp $@

BUILT_SOURCES = na-marshal.c na-marshal.h


EXTRA_DIST = \
	na-marshal.list


