AC_PREREQ(2.53)
AC_INIT(waterline, 0.6.0-alpha, http://dev.make-linux.org/projects/waterline)
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
AC_PROG_INTLTOOL(, [no-xml])
AM_PROG_CC_C_O

# Checks for libraries.
pkg_modules="gtk+-2.0 >= 2.24.0 \
             gio-unix-2.0 \
             gthread-2.0 \
             gmodule-2.0 \
             jansson >= 2.4 \
             sde-utils-1.0 >= 0.1 \
             sde-utils-jansson-1.0 >= 0.1 \
             sde-utils-gtk-1.0 >= 0.1"
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


AC_ARG_ENABLE([alsa],
    AS_HELP_STRING([--disable-alsa],
                    [compile ALSA providers (default: enable)]),
            compile_alsa=$enableval, compile_alsa="yes")
if test x"$compile_alsa" = "xyes"; then
    AC_CHECK_HEADERS([alsa/asoundlib.h],
                     [AC_CHECK_LIB([asound],
                                   [snd_seq_open],
                                   compile_alsa=yes,compile_alsa=no)],
                          compile_alsa=no)
fi
AM_CONDITIONAL(BUILD_ALSA_PLUGINS, test x$compile_alsa = xyes)
dnl FIXME: check OSS existence
#AM_CONDITIONAL(BUILD_OSS_PLUGINS, test x$compile_alsa = xno)

AC_ARG_ENABLE([plugins-loading],
    AS_HELP_STRING([--disable-plugins-loading],
               [disable plugin loading (default: enable)]),
               plugins_loading=$enableval, plugins_loading="yes")

AM_CONDITIONAL(BUILD_PLUGIN_LOADER, test x$plugins_loading = xyes)

if test x"$plugins_loading" = "xno"; then
    AC_DEFINE(DISABLE_PLUGINS_LOADING, [1], [Disable plugin loading])
fi

dnl Here are plugin listing.
#ALL_PLUGINS_LIST="taskbar netstatus volume volumealsa cpu deskno batt kbled xkb xkbh thermal cpufreq notification_area"
ALL_PLUGINS_LIST="taskbar netstatus volume volumealsa cpu deskno batt kbled xkb thermal cpufreq notification_area"
AC_SUBST(ALL_PLUGINS_LIST)

AC_MSG_CHECKING([which plugins should be built dynamically])
AC_ARG_WITH(plugins,
[  --with-plugins=<list>  compile dynamic plugins in <list> ]
[                         plugins may be comma separated ]
[                         'all' builds all plugins (default), 'none' builds none ]
[                         Possible plugins are: ]
[                         taskbar, notification_area, netstatus, volume, volumealsa,]
[                         cpu, deskno, batt, kbled, xkb, thermal, cpufreq], plugins="$withval",[plugins="all"])

if test x"$plugins" != xall -a x"$plugins" != xnone; then
    if test x"$plugins_loading" = xno; then
        AC_MSG_ERROR(
        [--disable-plugins-loading and --with-plugins conflict each other.])
    fi
fi

for name in $ALL_PLUGINS_LIST
do
    eval plugin_$name=
done

if test x"$plugins" = xall; then
    for name in $ALL_PLUGINS_LIST
    do
        eval plugin_$name="$name"
    done

    AC_MSG_RESULT(all)
else
    if test "$plugins" != "none"; then
        plugins=`echo $plugins | sed 's/,/ /g'`
        for plugin in $plugins
        do
            valid=no
            for name in $ALL_PLUGINS_LIST
            do
                if test x"$name" = x"$plugin" ; then
                    eval plugin_$name="$name"
                    valid=yes
                fi
            done
            if test "$valid" = "no" ; then
                echo "Unknown plugin $plugin."
                exit 1
            fi
        done
        AC_MSG_RESULT($plugins)
    fi
fi

dnl FIXME: OSS/ALSA checking
#if test x"$compile_alsa" = "xno"; then
#    if test ! -z $plugin_volumealsa; then
#        AC_MSG_RESULT([no ALSA headers/libraries found!])
#        plugin_volumealsa=
#    fi
#else
#    dnl force OSS plugin to <null>
#    plugin_volume=
#fi


PLUGINS_LIST=""
for name in $ALL_PLUGINS_LIST
do
    eval 'PLUGINS_LIST="$PLUGINS_LIST $plugin_'"$name"'"'
done

AC_SUBST(PLUGINS_LIST)


if test x"$plugin_notification_area" != x; then
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


GETTEXT_PACKAGE=waterline
#AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION(0.14.1)
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [Gettext package.])

ALL_LINGUAS="ru"
AM_GLIB_GNU_GETTEXT

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
    src/libexec/Makefile
    src/plugins/Makefile
    src/plugins/taskbar/Makefile
    src/plugins/notification_area/Makefile
    src/plugins/netstatus/Makefile
    src/plugins/volume/Makefile
    src/plugins/volumealsa/Makefile
    src/plugins/cpu/Makefile
    src/plugins/deskno/Makefile
    src/plugins/batt/Makefile
    src/plugins/kbled/Makefile
    src/plugins/xkb/Makefile
    src/plugins/xkbh/Makefile
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
    if test x"$plugin_deskno" != x; then
        AC_MSG_RESULT([    deskno - Display desktop number])
    fi
    if test x"$plugin_batt" != x; then
        AC_MSG_RESULT([    batt - Monitor battery status])
    fi
    if test x"$plugin_kbled" != x; then
        AC_MSG_RESULT([    kbled - Indicator for CapsLock, NumLock, and ScrLock])
    fi
    if test x"$plugin_xkb" != x; then
        AC_MSG_RESULT([    xkb - Keyboard layout switcher])
    fi
    if test x"$plugin_xkb" != x; then
        AC_MSG_RESULT([    xkbh - Keyboard layout handler])
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

