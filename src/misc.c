/**
 * Copyright (c) 2011-2012 Vadim Ushakov
 * Copyright (c) 2006 LxDE Developers, see the file AUTHORS for details.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/Xatom.h>
#include <X11/cursorfont.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gio/gio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <pwd.h>
#include <grp.h>
#include <sde-utils.h>

#include <waterline/global.h>
#include <waterline/misc.h>
#include <waterline/panel.h>
#include "panel_internal.h"
#include <waterline/x11_utils.h>
#include <waterline/gtkcompat.h>

/********************************************************************/

static GdkPixbuf * _gdk_pixbuf_new_from_file_at_scale(const char * file_name, int width, int height, gboolean keep_ratio);

/********************************************************************/

guint32 wtl_util_gdkcolor_to_uint32(const GdkColor * color)
{
    guint32 i;
    guint16 r, g, b;

    r = color->red * 0xFF / 0xFFFF;
    g = color->green * 0xFF / 0xFFFF;
    b = color->blue * 0xFF / 0xFFFF;

    i = r & 0xFF;
    i <<= 8;
    i |= g & 0xFF;
    i <<= 8;
    i |= b & 0xFF;

    return i;
}

/********************************************************************/

static GdkPixbuf * _gdk_pixbuf_new_from_file_at_scale(const char * file_path, int width, int height, gboolean keep_ratio)
{
    GdkPixbuf * icon = gdk_pixbuf_new_from_file_at_scale(file_path, width, height, keep_ratio, NULL);
    if (!icon)
        return NULL;

    /* It seems sometimes gdk_pixbuf_new_from_file_at_scale() does not scale pixbuf, so we do. */
    gulong w = gdk_pixbuf_get_width(icon);
    gulong h = gdk_pixbuf_get_height(icon);
    if ((width > 0 && w > width) || (height > 0 && h > height))
    {
        icon = su_gdk_pixbuf_scale_in_rect(icon, width, height, TRUE);
    }

    return icon;
}

/* Try to load an icon from a named file via the freedesktop.org data directories path.
 * http://standards.freedesktop.org/basedir-spec/basedir-spec-0.6.html */
static GdkPixbuf * load_icon_file(const char * file_name, int width, int height)
{
    GdkPixbuf * icon = NULL;
    const gchar ** dirs = (const gchar **) g_get_system_data_dirs();
    const gchar ** dir;
    for (dir = dirs; ((*dir != NULL) && (icon == NULL)); dir++)
    {
        char * file_path = g_build_filename(*dir, "pixmaps", file_name, NULL);
        icon = _gdk_pixbuf_new_from_file_at_scale(file_path, width, height, TRUE);
        g_free(file_path);
    }
    return icon;
}

/* Try to load an icon from the current theme. */
static GdkPixbuf * load_icon_from_theme(GtkIconTheme * theme, const char * icon_name, int width, int height)
{
    GdkPixbuf * icon = NULL;

    /* Look up the icon in the current theme. */
    GtkIconInfo * icon_info = NULL;
#if GLIB_CHECK_VERSION(2,20,0)
    if (icon_name && strlen(icon_name) > 7 && memcmp("GIcon ", icon_name, 6) == 0)
    {
        GIcon * gicon = g_icon_new_for_string(icon_name + 6, NULL);
        icon_info = gtk_icon_theme_lookup_by_gicon(theme, gicon, width, GTK_ICON_LOOKUP_USE_BUILTIN);
        g_object_unref(G_OBJECT(gicon));
    }
    else
#endif
    {
        icon_info = gtk_icon_theme_lookup_icon(theme, icon_name, height, GTK_ICON_LOOKUP_USE_BUILTIN);
    }

    if (icon_info != NULL)
    {
        /* If that succeeded, get the filename of the icon.
         * If that succeeds, load the icon from the specified file.
         * Otherwise, try to get the builtin icon. */
        const char * file = gtk_icon_info_get_filename(icon_info);
        if (file != NULL)
            icon = gdk_pixbuf_new_from_file(file, NULL);
        else
        {
            icon = gtk_icon_info_get_builtin_pixbuf(icon_info);
            g_object_ref(icon);
        }
        gtk_icon_info_free(icon_info);

        /* If the icon is not sized properly, take a trip through the scaler.
         * The lookup above takes the desired size, so we get the closest result possible. */
        if (icon != NULL)
        {
            if ((height != gdk_pixbuf_get_height(icon)) || (width != gdk_pixbuf_get_width(icon)))
            {
                /* Handle case of unspecified width; gdk_pixbuf_scale_simple does not. */
                if (width < 0)
                {
                    int pixbuf_width = gdk_pixbuf_get_width(icon);
                    int pixbuf_height = gdk_pixbuf_get_height(icon);
                    width = height * pixbuf_width / pixbuf_height;
                }
                GdkPixbuf * scaled = gdk_pixbuf_scale_simple(icon, width, height, GDK_INTERP_BILINEAR);
                g_object_unref(icon);
                icon = scaled;
            }
        }
    }
    return icon;
}

GdkPixbuf * wtl_load_icon(const char * name, int width, int height, gboolean use_fallback)
{
    return wtl_load_icon2(name, width, height, use_fallback, NULL);
}

GdkPixbuf* wtl_load_icon2(const char* name, int width, int height, gboolean use_fallback, gboolean * themed)
{
    GdkPixbuf * icon = NULL;

    if (themed)
        *themed = TRUE;

    if (name != NULL)
    {
        if (g_path_is_absolute(name))
        {
            /* Absolute path. */
            icon = _gdk_pixbuf_new_from_file_at_scale(name, width, height, TRUE);
            if (themed)
                *themed = FALSE;
        }
        else
        {
            /* Relative path. */
            GtkIconTheme * theme = gtk_icon_theme_get_default();
            char * suffix = strrchr(name, '.');
            if ((suffix != NULL)
            && ( (g_ascii_strcasecmp(&suffix[1], "png") == 0)
              || (g_ascii_strcasecmp(&suffix[1], "svg") == 0)
              || (g_ascii_strcasecmp(&suffix[1], "xpm") == 0)))
            {
                /* The file extension indicates it could be in the system pixmap directories. */
                icon = load_icon_file(name, width, height);
                if (icon == NULL)
                {
                    /* Not found.
                     * Let's remove the suffix, and see if this name can match an icon in the current icon theme. */
                    char * icon_name = g_strndup(name, suffix - name);
                    icon = load_icon_from_theme(theme, icon_name, width, height);
                    g_free(icon_name);
                }
            }
            else
            {
                 /* No file extension.  It could be an icon name in the icon theme. */
                 icon = load_icon_from_theme(theme, name, width, height);
            }
        }
    }

    /* Fall back to generic icons. */
    if ((icon == NULL) && (use_fallback))
    {
        GtkIconTheme * theme = gtk_icon_theme_get_default();
        icon = load_icon_from_theme(theme, "application-x-executable", width, height);
        if (icon == NULL)
            icon = load_icon_from_theme(theme, "gnome-mime-application-x-executable", width, height);
    }
    return icon;
}

/********************************************************************/

static void entry_dlg_response(GtkWidget * widget, int response, gpointer p)
{
    (void)p;

    EntryDialogCallback callback = (EntryDialogCallback)g_object_get_data(G_OBJECT(widget), "callback_func" );
    gpointer payload = g_object_get_data(G_OBJECT(widget), "payload" );
    GtkWidget * entry = g_object_get_data(G_OBJECT(widget), "entry" );

    char * value = NULL;

    if (response == GTK_RESPONSE_ACCEPT)
    {
        value = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
    }

    callback(value, payload);

    gtk_widget_destroy(widget);
}


GtkWidget* create_entry_dialog(const char * title, const char * description, const char * value, EntryDialogCallback callback, gpointer payload)
{
    GtkWidget* dlg = gtk_dialog_new_with_buttons( title, NULL, 0,
                                                 GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                                 GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                                                 NULL);

    panel_apply_icon(GTK_WINDOW(dlg));

    g_signal_connect( dlg, "response", G_CALLBACK(entry_dlg_response), NULL);

    g_object_set_data( G_OBJECT(dlg), "callback_func", callback);
    g_object_set_data( G_OBJECT(dlg), "payload", payload);

    GtkWidget* label = NULL;

    if (description)
        label = gtk_label_new(description);
    GtkWidget* entry = gtk_entry_new();

    g_object_set_data( G_OBJECT(dlg), "entry", entry);

    if (value)
        gtk_entry_set_text(GTK_ENTRY(entry), value);

    if (label)
        gtk_box_pack_start( GTK_BOX(GTK_DIALOG(dlg)->vbox), label, FALSE, FALSE, 2 );
    gtk_box_pack_start( GTK_BOX(GTK_DIALOG(dlg)->vbox), entry, FALSE, FALSE, 2 );

    gtk_widget_show_all( dlg );

    return dlg;
}

/********************************************************************/

void wtl_show_error_message(GtkWindow * parent_window, const char * message)
{
    GtkWidget * dialog = gtk_message_dialog_new(parent_window,
                                                GTK_DIALOG_MODAL,
                                                GTK_MESSAGE_ERROR,
                                                GTK_BUTTONS_OK, "%s", message);
    gtk_dialog_run((GtkDialog *) dialog);
    gtk_widget_destroy(dialog);
}

/********************************************************************/

void bring_to_current_desktop(GtkWidget * win)
{
    GdkWindow * gdk_window = gtk_widget_get_window(win);
    Window w = GDK_WINDOW_XID(gdk_window);
    int window_desktop = get_net_wm_desktop(w);
    int current_desktop = get_net_current_desktop();
    if (window_desktop != 0xFFFFFFFF && window_desktop != current_desktop)
    {
        set_net_wm_desktop(w, current_desktop);
    }

    gtk_window_present(GTK_WINDOW(win));
}

/********************************************************************/

/* Get the declaration of strmode.  */
# if HAVE_DECL_STRMODE
#  include <string.h> /* MacOS X, FreeBSD, OpenBSD */
#  include <unistd.h> /* NetBSD */
# endif

# if !HAVE_DECL_STRMODE
static void strmode (mode_t mode, char *str);
# endif

static void filemodestring (struct stat const *statp, char *str);

static char * human_access (struct stat const *statbuf)
{
    static char modebuf[12];
    filemodestring (statbuf, modebuf);
    modebuf[10] = 0;
    return modebuf;
}


gchar * wtl_tooltip_for_file_stat(struct stat * stat_data)
{
    setpwent ();
    struct passwd * pw_ent = getpwuid (stat_data->st_uid);
    gchar * s_user = g_strdup (pw_ent ? pw_ent->pw_name : "UNKNOWN");

    setgrent ();
    struct group * gw_ent = getgrgid (stat_data->st_gid);
    gchar * s_group = g_strdup (gw_ent ? gw_ent->gr_name : "UNKNOWN");

    gchar * tooltip = NULL;

    if (stat_data->st_uid == stat_data->st_gid && strcmp(s_user, s_group) == 0)
    {
        tooltip = g_strdup_printf(_("%'llu bytes, %s %s (%04o)"),
            (unsigned long long)stat_data->st_size,
            s_user,
            human_access(stat_data), (unsigned int)stat_data->st_mode);
    }
    else
    {
        tooltip = g_strdup_printf(_("%'llu bytes, %s:%s %s (%04o)"),
            (unsigned long long)stat_data->st_size,
            s_user, s_group,
            human_access(stat_data), (unsigned int)stat_data->st_mode);
    }

    g_free(s_user);
    g_free(s_group);

    return tooltip;
}

/********************************************************************/

/* filemode.c -- make a string describing file modes

   Copyright (C) 1985, 1990, 1993, 1998-2000, 2004, 2006, 2009-2012 Free
   Software Foundation, Inc.
*/

/* The following is for Cray DMF (Data Migration Facility), which is a
   HSM file system.  A migrated file has a 'st_dm_mode' that is
   different from the normal 'st_mode', so any tests for migrated
   files should use the former.  */
#if HAVE_ST_DM_MODE
# define IS_MIGRATED_FILE(statp) \
    (S_ISOFD (statp->st_dm_mode) || S_ISOFL (statp->st_dm_mode))
#else
# define IS_MIGRATED_FILE(statp) 0
#endif

#if ! HAVE_DECL_STRMODE

/* Return a character indicating the type of file described by
   file mode BITS:
   '-' regular file
   'b' block special file
   'c' character special file
   'C' high performance ("contiguous data") file
   'd' directory
   'D' door
   'l' symbolic link
   'm' multiplexed file (7th edition Unix; obsolete)
   'n' network special file (HP-UX)
   'p' fifo (named pipe)
   'P' port
   's' socket
   'w' whiteout (4.4BSD)
   '?' some other file type  */

static char ftypelet (mode_t bits)
{
  /* These are the most common, so test for them first.  */
  if (S_ISREG (bits))
    return '-';
  if (S_ISDIR (bits))
    return 'd';

  /* Other letters standardized by POSIX 1003.1-2004.  */
  if (S_ISBLK (bits))
    return 'b';
  if (S_ISCHR (bits))
    return 'c';
  if (S_ISLNK (bits))
    return 'l';
  if (S_ISFIFO (bits))
    return 'p';

  /* Other file types (though not letters) standardized by POSIX.  */
  if (S_ISSOCK (bits))
    return 's';

  /* Nonstandard file types.  */
/*  if (S_ISCTG (bits))
    return 'C';
  if (S_ISDOOR (bits))
    return 'D';
  if (S_ISMPB (bits) || S_ISMPC (bits))
    return 'm';
  if (S_ISNWK (bits))
    return 'n';
  if (S_ISPORT (bits))
    return 'P';
  if (S_ISWHT (bits))
    return 'w';
*/
  return '?';
}

/* Like filemodestring, but rely only on MODE.  */

static void strmode (mode_t mode, char *str)
{
  str[0] = ftypelet (mode);
  str[1] = mode & S_IRUSR ? 'r' : '-';
  str[2] = mode & S_IWUSR ? 'w' : '-';
  str[3] = (mode & S_ISUID
            ? (mode & S_IXUSR ? 's' : 'S')
            : (mode & S_IXUSR ? 'x' : '-'));
  str[4] = mode & S_IRGRP ? 'r' : '-';
  str[5] = mode & S_IWGRP ? 'w' : '-';
  str[6] = (mode & S_ISGID
            ? (mode & S_IXGRP ? 's' : 'S')
            : (mode & S_IXGRP ? 'x' : '-'));
  str[7] = mode & S_IROTH ? 'r' : '-';
  str[8] = mode & S_IWOTH ? 'w' : '-';
  str[9] = (mode & S_ISVTX
            ? (mode & S_IXOTH ? 't' : 'T')
            : (mode & S_IXOTH ? 'x' : '-'));
  str[10] = ' ';
  str[11] = '\0';
}

#endif /* ! HAVE_DECL_STRMODE */

/* filemodestring - fill in string STR with an ls-style ASCII
   representation of the st_mode field of file stats block STATP.
   12 characters are stored in STR.
   The characters stored in STR are:

   0    File type, as in ftypelet above, except that other letters are used
        for files whose type cannot be determined solely from st_mode:

            'F' semaphore
            'M' migrated file (Cray DMF)
            'Q' message queue
            'S' shared memory object
            'T' typed memory object

   1    'r' if the owner may read, '-' otherwise.

   2    'w' if the owner may write, '-' otherwise.

   3    'x' if the owner may execute, 's' if the file is
        set-user-id, '-' otherwise.
        'S' if the file is set-user-id, but the execute
        bit isn't set.

   4    'r' if group members may read, '-' otherwise.

   5    'w' if group members may write, '-' otherwise.

   6    'x' if group members may execute, 's' if the file is
        set-group-id, '-' otherwise.
        'S' if it is set-group-id but not executable.

   7    'r' if any user may read, '-' otherwise.

   8    'w' if any user may write, '-' otherwise.

   9    'x' if any user may execute, 't' if the file is "sticky"
        (will be retained in swap space after execution), '-'
        otherwise.
        'T' if the file is sticky but not executable.

   10   ' ' for compatibility with 4.4BSD strmode,
        since this interface does not support ACLs.

   11   '\0'.  */

static void filemodestring (struct stat const *statp, char *str)
{
  strmode (statp->st_mode, str);

  if (S_TYPEISSEM (statp))
    str[0] = 'F';
  else if (IS_MIGRATED_FILE (statp))
    str[0] = 'M';
  else if (S_TYPEISMQ (statp))
    str[0] = 'Q';
  else if (S_TYPEISSHM (statp))
    str[0] = 'S';
/*  else if (S_TYPEISTMO (statp))
    str[0] = 'T';*/
}

/********************************************************************/

/* This following function restore_grabs is taken from menu.c of
 * gnome-panel.
 */
/*most of this function stolen from the real gtk_menu_popup*/
void restore_grabs(GtkWidget *w, gpointer data)
{
    GtkWidget *menu_item = data;
    GtkMenu *menu = GTK_MENU(menu_item->parent);
    GtkWidget *xgrab_shell;
    GtkWidget *parent;

    /* Find the last viewable ancestor, and make an X grab on it
    */
    parent = GTK_WIDGET (menu);
    xgrab_shell = NULL;
    while (parent)
    {
        gboolean viewable = TRUE;
        GtkWidget *tmp = parent;

        while (tmp)
        {
            if (!gtk_widget_get_mapped(tmp))
            {
                viewable = FALSE;
                break;
            }
            tmp = tmp->parent;
        }

        if (viewable)
            xgrab_shell = parent;

        parent = GTK_MENU_SHELL (parent)->parent_menu_shell;
    }

    /*only grab if this HAD a grab before*/
    if (xgrab_shell && (GTK_MENU_SHELL (xgrab_shell)->have_xgrab))
    {
        if (gdk_pointer_grab (xgrab_shell->window, TRUE,
                    GDK_BUTTON_PRESS_MASK |
                    GDK_BUTTON_RELEASE_MASK |
                    GDK_ENTER_NOTIFY_MASK |
                    GDK_LEAVE_NOTIFY_MASK,
                    NULL, NULL, 0) == 0)
        {
            if (gdk_keyboard_grab (xgrab_shell->window, TRUE,
                    GDK_CURRENT_TIME) == 0)
                GTK_MENU_SHELL (xgrab_shell)->have_xgrab = TRUE;
            else
                gdk_pointer_ungrab (GDK_CURRENT_TIME);
        }
    }
    gtk_grab_add (GTK_WIDGET (menu));
}

/********************************************************************/

gboolean is_my_own_window(Window window)
{
    return !!gdk_window_lookup(window);
}

/********************************************************************/

void color_parse_d(const char * src, double dst[3])
{
    GdkColor color;

    gdk_color_parse(src, &color);

    dst[0] = ((double) color.red) / 65535.0;
    dst[1] = ((double) color.green) / 65535.0;
    dst[2] = ((double) color.blue) / 65535.0;
}

/********************************************************************/

void wtl_util_gdkrgba_to_gdkcolor(GdkRGBA * rgba, GdkColor * color, guint16 * alpha)
{
    if (!rgba)
        return;

    if (color)
    {
        int r = rgba->red * 65535;
        if (r < 0)
            color->red = 0;
        else if (r > 65535)
            color->red = 65535;
        else
            color->red = r;

        int g = rgba->green * 65535;
        if (g < 0)
            color->green = 0;
        else if (g > 65535)
            color->green = 65535;
        else
            color->green = g;

        int b = rgba->blue * 65535;
        if (b < 0)
            color->blue = 0;
        else if (b > 65535)
            color->blue = 65535;
        else
            color->blue = b;
    }

    if (alpha)
    {
        int a = rgba->alpha * 65535;
        if (a < 0)
            *alpha = 0;
        else if (a > 65535)
            *alpha = 65535;
        else
            *alpha = a;
    }

}

/********************************************************************/

void wtl_util_gdkcolor_to_gdkrgba(GdkRGBA * rgba, GdkColor * color, guint16 * alpha)
{
    if (!rgba)
        return;

    if (color)
    {
        rgba->red   = ((double) color->red) / 65535.0;
        rgba->green = ((double) color->green) / 65535.0;
        rgba->blue  = ((double) color->blue) / 65535.0;
    }

    if (alpha)
    {
        rgba->alpha = ((double) *alpha) / 65535.0;
    }

}

/********************************************************************/
