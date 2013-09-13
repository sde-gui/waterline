## Process this file with automake to produce Makefile.in

SUBDIRS = plugins libconfigurator

bin_PROGRAMS = waterline waterlinectl

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/src/plugins \
	-DPACKAGE_LIB_DIR=\""$(libdir)"\" \
	-DPACKAGE_LIBEXEC_DIR=\""$(libexecdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DPACKAGE_INSTALLATION_PREFIX=\""$(prefix)"\" \
	$(PACKAGE_CFLAGS) \
	$(XLIB_CFLAGS) \
	$(G_CAST_CHECKS)

BUILTIN_PLUGINS = $(top_builddir)/src/plugins/libbuiltin_plugins.a

if ENABLE_MENU_CACHE
MENU_SOURCES = \
	gtk-run.c \
	menu-policy.h menu-policy.c
endif

waterline_SOURCES = \
	misc.c \
	fb_button.c \
	defaultapplications.c \
	libfm.c \
	window-icons.c \
	openbox-integration.c \
	bg.c bg.h  \
	commands.c commands.h \
	generic_config_dlg.c \
	Xsupport.c \
	configurator.c \
	ev.c \
	panel_config.c \
	panel_menu.c \
	panel.c panel_internal.h panel_private.h \
	plugin.c plugin_internal.h plugin_private.h \
	paths.c \
	de.c \
	$(MENU_SOURCES)

if BUILD_PLUGIN_LOADER
DYNAMIC_FLAGS = -export-dynamic
waterline_includedir = $(pkgincludedir)/waterline
waterline_include_HEADERS = \
	$(top_srcdir)/include/waterline/waterline/dbg.h \
	$(top_srcdir)/include/waterline/waterline/defaultapplications.h \
	$(top_srcdir)/include/waterline/waterline/ev.h \
	$(top_srcdir)/include/waterline/waterline/fb_button.h \
	$(top_srcdir)/include/waterline/waterline/global.h \
	$(top_srcdir)/include/waterline/waterline/gtkcompat.h \
	$(top_srcdir)/include/waterline/waterline/menu-cache-compat.h \
	$(top_srcdir)/include/waterline/waterline/libfm.h \
	$(top_srcdir)/include/waterline/waterline/misc.h \
	$(top_srcdir)/include/waterline/waterline/panel.h \
	$(top_srcdir)/include/waterline/waterline/paths.h \
	$(top_srcdir)/include/waterline/waterline/plugin.h \
	$(top_srcdir)/include/waterline/waterline/typedef.h \
	$(top_srcdir)/include/waterline/waterline/Xsupport.h
endif
waterline_LDFLAGS = $(DYNAMIC_FLAGS)

waterline_LDADD = \
		$(BUILTIN_PLUGINS) \
		$(PACKAGE_LIBS) \
		$(X11_LIBS) \
		$(INTLLIBS)

waterlinectl_SOURCES = waterlinectl.c
waterlinectl_LDADD = $(X11_LIBS) $(INTLLIBS)


EXTRA_DIST = \
	plugins/icon.xpm
