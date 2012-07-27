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
	global.h \
	typedefs.h \
	misc.c misc.h \
	defaultapplications.c defaultapplications.h \
	libfm.c libfm.h \
	bg.c bg.h  \
	commands.c commands.h \
	configparser.c configparser.h \
	generic_config_dlg.c \
	Xsupport.c Xsupport.h \
	pixbuf-stuff.c pixbuf-stuff.h \
	configurator.c \
	dbg.c dbg.h \
	ev.c ev.h \
	icon-grid.h icon-grid.c \
	panel.c panel.h panel_private.h \
	plugin.c plugin.h plugin_private.h \
	paths.c paths.h \
	$(MENU_SOURCES)

if BUILD_PLUGIN_LOADER
DYNAMIC_FLAGS = -export-dynamic
lxpanelx_includedir = $(pkgincludedir)
lxpanelx_include_HEADERS = $(top_srcdir)/include/lxpanelx/plugin.h
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
