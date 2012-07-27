## Process this file with automake to produce Makefile.in

SUBDIRS = plugins libexec

bin_PROGRAMS = lxpanelx lxpanelxctl

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/src/plugins \
	-DPACKAGE_LIB_DIR=\""$(libdir)"\" \
	-DPACKAGE_LIBEXEC_DIR=\""$(libexecdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	$(PACKAGE_CFLAGS) \
	$(XLIB_CFLAGS) \
	$(G_CAST_CHECKS)

BUILTIN_PLUGINS = $(top_builddir)/src/plugins/libbuiltin_plugins.a

if ENABLE_MENU_CACHE
MENU_SOURCES = \
	gtk-run.c \
	menu-policy.h menu-policy.c
endif

lxpanelx_SOURCES = \
	misc.c \
	defaultapplications.c \
	libfm.c \
	bg.c bg.h  \
	commands.c commands.h \
	configparser.c \
	generic_config_dlg.c \
	Xsupport.c \
	pixbuf-stuff.c \
	configurator.c \
	dbg.c \
	ev.c \
	icon-grid.c \
	panel.c panel_internal.h panel_private.h \
	plugin.c plugin_internal.h plugin_private.h \
	paths.c \
	$(MENU_SOURCES)

if BUILD_PLUGIN_LOADER
DYNAMIC_FLAGS = -export-dynamic
lxpanelx_includedir = $(pkgincludedir)/lxpanelx
lxpanelx_include_HEADERS = \
	$(top_srcdir)/include/lxpanelx/lxpanelx/configparser.h \
	$(top_srcdir)/include/lxpanelx/lxpanelx/dbg.h \
	$(top_srcdir)/include/lxpanelx/lxpanelx/defaultapplications.h \
	$(top_srcdir)/include/lxpanelx/lxpanelx/ev.h \
	$(top_srcdir)/include/lxpanelx/lxpanelx/global.h \
	$(top_srcdir)/include/lxpanelx/lxpanelx/gtkcompat.h \
	$(top_srcdir)/include/lxpanelx/lxpanelx/icon-grid.h \
	$(top_srcdir)/include/lxpanelx/lxpanelx/libfm.h \
	$(top_srcdir)/include/lxpanelx/lxpanelx/misc.h \
	$(top_srcdir)/include/lxpanelx/lxpanelx/panel.h \
	$(top_srcdir)/include/lxpanelx/lxpanelx/paths.h \
	$(top_srcdir)/include/lxpanelx/lxpanelx/pixbuf-stuff.h \
	$(top_srcdir)/include/lxpanelx/lxpanelx/plugin.h \
	$(top_srcdir)/include/lxpanelx/lxpanelx/typedef.h \
	$(top_srcdir)/include/lxpanelx/lxpanelx/Xsupport.h
endif
lxpanelx_LDFLAGS = $(DYNAMIC_FLAGS)

lxpanelx_LDADD = \
		$(BUILTIN_PLUGINS) \
		$(PACKAGE_LIBS) \
		$(X11_LIBS) \
		$(INTLLIBS)

lxpanelxctl_SOURCES = lxpanelxctl.c lxpanelxctl.h
lxpanelxctl_LDADD = $(X11_LIBS) $(INTLLIBS)

noinst_PROGRAMS=xml-purge
xml_purge_SOURCES=xml-purge.c
xml_purge_CFLAGS=@PACKAGE_CFLAGS@
xml_purge_LDADD=@PACKAGE_LIBS@


EXTRA_DIST = \
	plugins/icon.xpm
