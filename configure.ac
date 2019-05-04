AC_PREREQ(2.53)
AC_INIT(waterline, 0.6.0-alpha, http://dev.make-linux.org/projects/waterline)
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE([enable])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES])
AC_CONFIG_SRCDIR([src/bg.c])
AC_CONFIG_HEADER([config.h])

# Force to dynamic
AC_DISABLE_STATIC

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_LIBTOOL
AM_PROG_CC_C_O


dnl ================================================================
dnl Gettext stuff.
dnl ================================================================

IT_PROG_INTLTOOL([0.50.1])

AM_GNU_GETTEXT_VERSION([0.18])
AM_GNU_GETTEXT([external])

AC_SUBST([GETTEXT_PACKAGE], [waterline])
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], "$GETTEXT_PACKAGE", [Define to the gettext package name.])

dnl ================================================================

# Checks for libraries.
pkg_modules="gtk+-2.0 >= 2.24.0 \
             gio-unix-2.0 \
             gthread-2.0 \
             gmodule-2.0 \
             jansson >= 2.2 \
             sde-utils-0 >= 0.1 \
             sde-utils-x11-0 >= 0.1 \
             sde-utils-jansson-0 >= 0.1 \
             sde-utils-gtk-0 >= 0.1"
#             libstartup-notification-1.0"
PKG_CHECK_MODULES(PACKAGE, [$pkg_modules])
AC_SUBST(PACKAGE_CFLAGS)
AC_SUBST(PACKAGE_LIBS)

pkg_modules="x11 xcomposite"
PKG_CHECK_MODULES(X11, [$pkg_modules])
AC_SUBST(X11_CFLAGS)
AC_SUBST(X11_LIBS)

# menu cache

AC_ARG_WITH(menucache, [  --without-menucache     Compile without menucache support])
if test "x$with_menucache" != "xno"; then
	pkg_modules="libmenu-cache"
	PKG_CHECK_MODULES(MENU_CACHE, [$pkg_modules],
			  enable_menu_cache=yes, enable_menu_cache=no)
	if test x"$enable_menu_cache" = "xno"; then
		AC_WARN([No libmenu-cache found.  All menu related routines are not built.])
		AC_DEFINE(DISABLE_MENU, [1], [Disable menu related routines])
	fi
	AC_SUBST(MENU_CACHE_CFLAGS)
	AC_SUBST(MENU_CACHE_LIBS)
    MENU_CACHE_VERSION="$(pkg-config --modversion libmenu-cache)"
    old_IFS="$IFS"
    IFS=.
    read MENU_CACHE_MAJOR_VERSION MENU_CACHE_MINOR_VERSION MENU_CACHE_MICRO_VERSION <<enddoc
$MENU_CACHE_VERSION
enddoc
    IFS="$old_IFS"
    AC_SUBST(MENU_CACHE_VERSION)
    AC_SUBST(MENU_CACHE_MAJOR_VERSION)
    AC_SUBST(MENU_CACHE_MINOR_VERSION)
    AC_SUBST(MENU_CACHE_MICRO_VERSION)
else
	enable_menu_cache=no
	AC_WARN([libmenu-cache disabled.  All menu related routines are not built.])
	AC_DEFINE(DISABLE_MENU, [1], [Disable menu related routines])
fi
AM_CONDITIONAL(ENABLE_MENU_CACHE, test x"$enable_menu_cache" = x"yes")

AC_DEFINE_UNQUOTED([MENU_CACHE_MAJOR_VERSION],[$MENU_CACHE_MAJOR_VERSION], [-])
AC_DEFINE_UNQUOTED([MENU_CACHE_MINOR_VERSION],[$MENU_CACHE_MINOR_VERSION], [-])
AC_DEFINE_UNQUOTED([MENU_CACHE_MICRO_VERSION],[$MENU_CACHE_MICRO_VERSION], [-])

PACKAGE_CFLAGS="$PACKAGE_CFLAGS $MENU_CACHE_CFLAGS"' -I$(top_srcdir)/include/waterline/'
PACKAGE_LIBS="$PACKAGE_LIBS $MENU_CACHE_LIBS -lutil"

# man

AC_ARG_ENABLE(man,
	[AC_HELP_STRING([--enable-man],
		[regenerate roff man pages from Docbook @<:@default=no@:>@])],
	[enable_man="${enableval}"],
	[enable_man=no]
)

# debug

AC_ARG_ENABLE([debug],
    AS_HELP_STRING([--enable-debug],
               [enable debug build (default: disable)]),
               debug=$enableval, debug="no")
if test x"$debug" = "xyes"; then
    CFLAGS="$CFLAGS -DDEBUG -Og -g"
fi

# cast checks

AC_ARG_ENABLE([cast-checks],
    AS_HELP_STRING([--enable-cast-checks],
               [enable Glib casting checks (default: disable)]),
               cast_checks=$enableval, cast_checks="no")
G_CAST_CHECKS=""
if test x"$cast_checks" = "xno"; then
    G_CAST_CHECKS="-DG_DISABLE_CAST_CHECKS"
fi
AC_SUBST(G_CAST_CHECKS)

# 

AC_ARG_ENABLE([gtk-checks],
    AS_HELP_STRING([--enable-gtk-checks],
               [defines GTK_DISABLE_SINGLE_INCLUDES, GDK_DISABLE_DEPRECATED, GTK_DISABLE_DEPRECATED, GSEAL_ENABLE]),
               gtk_checks=$enableval, gtk_checks="no")
GTK_CHECKS=""
if test x"$gtk_checks" = "xyes"; then
    GTK_CHECKS="-DGTK_DISABLE_SINGLE_INCLUDES -DGDK_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED -DGSEAL_ENABLE"
fi
#AC_SUBST(GTK_CHECKS)

PACKAGE_CFLAGS="$PACKAGE_CFLAGS $GTK_CHECKS"


_save_cflags="$CFLAGS"
CFLAGS="-Werror-implicit-function-declaration"
AC_MSG_CHECKING([whether $CC accepts $CFLAGS])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])],
    [AC_MSG_RESULT([yes])]
    CFLAGS="$_save_cflags $CFLAGS",
    [AC_MSG_RESULT([no])]
    CFLAGS="$_save_cflags"
)


dnl linker tweaking
# The function of the link flag --as-needed is to prevent unnecesary linking.
# Example: A -> B -> C
# Normally, A would link to B and also depend on C, this is of cource
# unnecesary. In this situation, however we do need to link to C, so this
# must be done explicitly. This flag comes in handy when a library ABI
# is changed, minimizing the amount of recompilations needed.
AC_MSG_CHECKING([whether $LD accepts --as-needed])
case `$LD --as-needed -v 2>&1 </dev/null` in
*GNU* | *'with BFD'*)
    LDFLAGS="$LDFLAGS -Wl,--as-needed"
    AC_MSG_RESULT([yes])
    ;;
*)
    AC_MSG_RESULT([no])
    ;;
esac

dnl linker optimizations
AC_MSG_CHECKING([whether $LD accepts -O1])
case `$LD -O1 -v 2>&1 </dev/null` in
*GNU* | *'with BFD'*)
  LDFLAGS="$LDFLAGS -Wl,-O1"
  AC_MSG_RESULT([yes])
  ;;
*)
  AC_MSG_RESULT([no])
  ;;
esac
AC_MSG_CHECKING([whether $LD accepts -Bsymbolic-functions])
case `$LD -Bsymbolic-functions -v 2>&1 </dev/null` in
*GNU* | *'with BFD'*)
  LDFLAGS="$LDFLAGS -Wl,-Bsymbolic-functions"
  AC_MSG_RESULT([yes])
  ;;
*)
  AC_MSG_RESULT([no])
  ;;
esac
AC_MSG_CHECKING([whether $LD accepts --sort-common])
case `$LD --sort-common -v 2>&1 </dev/null` in
*GNU* | *'with BFD'*)
  LDFLAGS="$LDFLAGS -Wl,--sort-common"
  AC_MSG_RESULT([yes])
  ;;
*)
  AC_MSG_RESULT([no])
  ;;
esac

dnl FIXME: filter for undefined symbols in plugins
# when passing "-z defs" to linker, we would better to make sure
# there are no undefined references.  However, we intend not to
# fix at the moment since we don't have spin-off libraray containing
# the necessary functions.
if test ! -z "$LDFLAGS"; then
    LDFLAGS=`echo "$LDFLAGS" | sed -e 's/-Wl,-z,defs//'`
fi

WATERLINE_MODULE="-avoid-version \
-rpath \$(pkglibdir)/plugins \
-no-undefined "'-export-symbols-regex "^[[^_]].*"'
AC_SUBST(WATERLINE_MODULE)

WATERLINE_INTERNAL_MODULE="-avoid-version \
-rpath \$(pkglibdir)/internals \
-no-undefined "'-export-symbols-regex "^[[^_]].*"'
AC_SUBST(WATERLINE_INTERNAL_MODULE)

##############################################################################

# FIXME: give this parameter somewhat more reasonable name

AC_ARG_ENABLE([plugins-loading],
    AS_HELP_STRING([--disable-plugins-loading],
               [disable plugin loading (default: enable)]),
               plugins_loading=$enableval, plugins_loading="yes")

AM_CONDITIONAL(BUILD_PLUGIN_LOADER, test x$plugins_loading = xyes)

if test x"$plugins_loading" = "xno"; then
    AC_DEFINE(DISABLE_PLUGINS_LOADING, [1], [Disable plugin loading])
fi

##############################################################################

# NOTE: Keep this in sync with --enable-plugin-* options
ALL_PLUGINS_LIST="taskbar pager show_desktop notification_area netstatus volume volumealsa cpu battery_indicator xkb_leds xkb_groups xkb_locks thermal cpufreq"
AC_SUBST(ALL_PLUGINS_LIST)

##############################################################################

# Plugin-specific checks.

for name in $ALL_PLUGINS_LIST ; do
    eval autodetected_plugin_$name=
done

# ALSA

SDE_WITH([alsa], [enable ALSA support for volumealsa plugin @<:@default=auto@:>@], [
    AC_CHECK_HEADERS([alsa/asoundlib.h], [
        AC_CHECK_LIB([asound], [snd_seq_open], [
            have_alsa=yes
        ])
    ])
])

AM_CONDITIONAL(BUILD_ALSA_PLUGINS, test x$have_alsa = xyes)
autodetected_plugin_volumealsa="$have_alsa"

# OSS

SDE_WITH([oss], [enable support of the Open Sound System for volume plugin @<:@default=auto@:>@], [
    AC_CHECK_HEADERS([sys/soundcard.h], have_oss="yes")
    AC_CHECK_HEADERS([linux/soundcard.h], have_oss="yes")
])

autodetected_plugin_volume="$have_oss"

# Non-portable plugins

# FIXME: The following plugins are currently implemented only for Linux.
# We check for the presence of <sys/sysinfo.h> to (hackishly) detect Linux and disable the plugins.
AC_CHECK_HEADERS([sys/sysinfo.h], sys_sysinfo_h="yes", sys_sysinfo_h="no")
autodetected_plugin_battery_indicator="$sys_sysinfo_h"
autodetected_plugin_cpu="$sys_sysinfo_h"
autodetected_plugin_cpufreq="$sys_sysinfo_h"
autodetected_plugin_netstatus="$sys_sysinfo_h"
autodetected_plugin_thermal="$sys_sysinfo_h"

##############################################################################

# Build the plugin list based on configure options

AC_MSG_CHECKING([which plugins should be built dynamically])
AC_ARG_ENABLE(plugins,
[  --enable-plugins=<list> the list of dynamically loadable plugins to be built:]
[                         'auto' autodetects which plugins should be build (default),]
[                         'all' builds all plugins,]
[                         'none' builds none.]
[                         Possible plugins are:]
[                         taskbar, pager, show_desktop, notification_area,]
[                         netstatus, volume, volumealsa, cpu, cpufreq,]
[                         battery_indicator, xkb_leds, xkb_groups, xkb_locks, thermal.]
[                         If any of --enable-plugin-* options are specified,]
[                         they take precedence over the list.],
    [enable_plugins="$enableval"],[enable_plugins="auto"])

for name in $ALL_PLUGINS_LIST ; do
    eval plugin_$name=no
done

if test x"$enable_plugins" = xnone ; then
    AC_MSG_RESULT(none)
elif test x"$enable_plugins" = xall ; then
    for name in $ALL_PLUGINS_LIST ; do
        eval plugin_$name=yes
    done
    AC_MSG_RESULT(all)
elif test x"$enable_plugins" = xauto ; then
    for name in $ALL_PLUGINS_LIST ; do
        if test x"$plugins_loading" = "xno"; then
            eval plugin_$name=no
        else
            eval plugin_$name=auto
        fi
    done
    AC_MSG_RESULT(auto)
else
    enable_plugins=`echo $enable_plugins | sed 's/,/ /g'`
    for plugin in $enable_plugins ; do
        valid=no
        for name in $ALL_PLUGINS_LIST ; do
            if test x"$name" = x"$plugin" ; then
                eval plugin_$name=yes
                valid=yes
            fi
        done

        if test "$valid" = "no" ; then
            echo "Unknown plugin $plugin."
            exit 1
        fi
    done
    AC_MSG_RESULT($enable_plugins)
fi

AC_ARG_ENABLE([plugin-battery_indicator],
    AS_HELP_STRING([--enable-plugin-battery_indicator],
                   [build plugin: battery_indicator @<:@default=auto@:>@]),
            plugin_battery_indicator=$enableval)

AC_ARG_ENABLE([plugin-cpu],
    AS_HELP_STRING([--enable-plugin-cpu],
                   [build plugin: cpu @<:@default=auto@:>@]),
            plugin_cpu=$enableval)

AC_ARG_ENABLE([plugin-cpufreq],
    AS_HELP_STRING([--enable-plugin-cpufreq],
                   [build plugin: cpufreq @<:@default=auto@:>@]),
            plugin_cpufreq=$enableval)

AC_ARG_ENABLE([plugin-netstatus],
    AS_HELP_STRING([--enable-plugin-netstatus],
                   [build plugin: netstatus @<:@default=auto@:>@]),
            plugin_netstatus=$enableval)

AC_ARG_ENABLE([plugin-notification_area],
    AS_HELP_STRING([--enable-plugin-notification_area],
                   [build plugin: notification_area @<:@default=auto@:>@]),
            plugin_notification_area=$enableval)

AC_ARG_ENABLE([plugin-pager],
    AS_HELP_STRING([--enable-plugin-pager],
                   [build plugin: pager @<:@default=auto@:>@]),
            plugin_pager=$enableval)

AC_ARG_ENABLE([plugin-show_desktop],
    AS_HELP_STRING([--enable-plugin-show_desktop],
                   [build plugin: show_desktop @<:@default=auto@:>@]),
            plugin_show_desktop=$enableval)

AC_ARG_ENABLE([plugin-taskbar],
    AS_HELP_STRING([--enable-plugin-taskbar],
                   [build plugin: taskbar @<:@default=auto@:>@]),
            plugin_taskbar=$enableval)

AC_ARG_ENABLE([plugin-thermal],
    AS_HELP_STRING([--enable-plugin-thermal],
                   [build plugin: thermal @<:@default=auto@:>@]),
            plugin_thermal=$enableval)

AC_ARG_ENABLE([plugin-volume],
    AS_HELP_STRING([--enable-plugin-volume],
                   [build plugin: volume @<:@default=auto@:>@]),
            plugin_volume=$enableval)

AC_ARG_ENABLE([plugin-volumealsa],
    AS_HELP_STRING([--enable-plugin-volumealsa],
                   [build plugin: volumealsa @<:@default=auto@:>@]),
            plugin_volumealsa=$enableval)

AC_ARG_ENABLE([plugin-xkb_groups],
    AS_HELP_STRING([--enable-plugin-xkb_groups],
                   [build plugin: xkb_groups @<:@default=auto@:>@]),
            plugin_xkb_groups=$enableval)

AC_ARG_ENABLE([plugin-xkb_leds],
    AS_HELP_STRING([--enable-plugin-xkb_leds],
                   [build plugin: xkb_leds @<:@default=auto@:>@]),
            plugin_xkb_leds=$enableval)

AC_ARG_ENABLE([plugin-xkb_locks],
    AS_HELP_STRING([--enable-plugin-xkb_locks],
                   [build plugin: xkb_locks @<:@default=auto@:>@]),
            plugin_xkb_locks=$enableval)

# replace 'auto's with autodetected values
for name in $ALL_PLUGINS_LIST ; do
    val=`eval echo '$'plugin_$name`
    autodetected_val=`eval echo '$'autodetected_plugin_$name`
    if test x"$val" = xauto ; then
        if test x"$autodetected_val" = x ; then
            echo "plugin: $name: enabled by default"
            eval plugin_$name=yes
        elif test x"$autodetected_val" = xyes ; then
            echo "plugin: $name: enabled by autodetection"
            eval plugin_$name=yes
        else
            echo "plugin: $name: disabled by autodetection"
            eval plugin_$name=no
        fi
    elif test x"$val" = xyes ; then
        echo "plugin: $name: enabled by user"
        if test x"$autodetected_val" = xno ; then
            AC_MSG_ERROR([unsatisfied dependencies for plugin $name])
        fi
    else
        echo "plugin: $name: disabled by user"
    fi
done

# check for conflicting options (--disable-plugins-loading vs any DL plugins)
if test x"$plugins_loading" = xno; then
    for name in $ALL_PLUGINS_LIST ; do
        val=`eval echo '$'plugin_$name`
        if test x"$val" = xyes ; then
            AC_MSG_ERROR([--disable-plugins-loading cannot be used together with any dynamically loadable plugins.])
        fi
    done
fi

# generate the plugin list for Makefile
PLUGINS_LIST=""
for name in $ALL_PLUGINS_LIST ; do
    val=`eval echo '$'plugin_$name`
    if test x"$val" = xyes ; then
        PLUGINS_LIST="$PLUGINS_LIST $name"
    fi
done

AC_SUBST(PLUGINS_LIST)

##############################################################################

if test x"$plugin_notification_area" != x ; then
    AC_PATH_PROG(GLIB_GENMARSHAL, glib-genmarshal)
fi

# Checks for header files.
AC_PATH_X
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([locale.h stdlib.h string.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_STRUCT_TM

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_TYPE_SIGNAL
AC_FUNC_STAT
AC_FUNC_STRFTIME
AC_CHECK_FUNCS([bzero memset mkdir setlocale strchr])
AC_CHECK_DECLS_ONCE([strmode])

if test x"$enable_man" = x"yes"; then
	AC_PATH_PROG([XSLTPROC], [xsltproc])
	if test -z "$XSLTPROC"; then
		enable_man=no
	fi

	dnl check for DocBook DTD and stylesheets in the local catalog.
#	JH_CHECK_XML_CATALOG([-//OASIS//DTD DocBook XML V4.1.2//EN],
#		[DocBook XML DTD V4.1.2], [], enable_man=no)
#	JH_CHECK_XML_CATALOG([http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl],
#		[DocBook XSL Stylesheets >= 1.70.1], [], enable_man=no)
fi

AM_CONDITIONAL(ENABLE_REGENERATE_MAN, test "x$enable_man" != "xno")

AC_CONFIG_FILES([
    waterline.pc
    Makefile
    src/Makefile
    src/libconfigurator/Makefile
    src/plugins/Makefile
    src/plugins/taskbar/Makefile
    src/plugins/pager/Makefile
    src/plugins/show_desktop/Makefile
    src/plugins/notification_area/Makefile
    src/plugins/netstatus/Makefile
    src/plugins/volume/Makefile
    src/plugins/volumealsa/Makefile
    src/plugins/cpu/Makefile
    src/plugins/battery_indicator/Makefile
    src/plugins/xkb_leds/Makefile
    src/plugins/xkb_groups/Makefile
    src/plugins/xkb_locks/Makefile
    src/plugins/thermal/Makefile
    src/plugins/cpufreq/Makefile
    po/Makefile.in
    data/Makefile
])
AC_OUTPUT


echo
echo waterline........................ : Version $VERSION
echo
echo Prefix........................... : $prefix
if test x$plugins_loading = xyes; then
echo Building dynamic plugins:
    if test x"$plugin_taskbar" != x; then
        AC_MSG_RESULT([    taskbar - Taskbar])
    fi
    if test x"$plugin_pager" != x; then
        AC_MSG_RESULT([    pager - Desktop Pager])
    fi
    if test x"$plugin_show_desktop" != x; then
        AC_MSG_RESULT([    show_desktop - Show Desktop button])
    fi
    if test x"$plugin_notification_area" != x; then
        AC_MSG_RESULT([    notification_area - notification area])
    fi
    if test x"$plugin_netstatus" != x; then
        AC_MSG_RESULT([    netstatus - Monitor networking status])
    fi
    if test x"$plugin_volume" != x; then
        AC_MSG_RESULT([    volume - Display and adjust volume of sound card for OSS])
    fi
    if test x"$plugin_volumealsa" != x; then
        AC_MSG_RESULT([    volumealsa - Display and adjust volume of sound card for ALSA])
    fi
    if test x"$plugin_cpu" != x; then
        AC_MSG_RESULT([    cpu - Display CPU loading])
    fi
    if test x"$plugin_battery_indicator" != x; then
        AC_MSG_RESULT([    battery_indicator - Battery charge level monitoring plugin])
    fi
    if test x"$plugin_xkb_leds" != x; then
        AC_MSG_RESULT([    xkb_leds - Indicator of Caps Lock, Num Lock, and Scroll Lock LEDs])
    fi
    if test x"$plugin_xkb_locks" != x; then
        AC_MSG_RESULT([    xkb_locks - Indicator of Caps Lock, Num Lock, and Scroll Lock modifiers])
    fi
    if test x"$plugin_xkb_groups" != x; then
        AC_MSG_RESULT([    xkb_groups - Keyboard layout switcher])
    fi
    if test x"$plugin_thermal" != x; then
        AC_MSG_RESULT([    thermal - Temperature monitor])
    fi
    if test x"$plugin_cpufreq" != x; then
        AC_MSG_RESULT([    cpufreq - CpuFreq frontend])
    fi
echo "PLUGINS_LIST = $PLUGINS_LIST"
else
echo Dynamic loader for plugins....... : disabled.
fi

echo
echo "PACKAGE_CFLAGS     = $PACKAGE_CFLAGS"
echo "PACKAGE_LIBS       =  $PACKAGE_LIBS"
echo "X11_CFLAGS         =  $X11_CFLAGS"
echo "X11_LIBS           =  $X11_LIBS"
#echo "MENU_CACHE_CFLAGS  =  $MENU_CACHE_CFLAGS"
#echo "MENU_CACHE_LIBS    =  $MENU_CACHE_LIBS"
echo "MENU_CACHE_VERSION =  $MENU_CACHE_VERSION"
echo "LDFLAGS            =  $LDFLAGS"
echo "CFLAGS             =  $CFLAGS"
echo "CC                 =  $CC"
echo "LD                 =  $LD"
echo "AR                 =  $AR"
