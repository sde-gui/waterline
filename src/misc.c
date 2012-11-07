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

#include <lxpanelx/global.h>
#include <lxpanelx/misc.h>
#include <lxpanelx/panel.h>
#include "panel_internal.h"
#include <lxpanelx/Xsupport.h>
#include <lxpanelx/dbg.h>

#include <lxpanelx/gtkcompat.h>

/********************************************************************/

static GdkPixbuf * _gdk_pixbuf_new_from_file_at_scale(const char * file_name, int width, int height, gboolean keep_ratio);

/********************************************************************/

int strempty(const char* s) {
    if (!s)
        return 1;
    while (*s == ' ' || *s == '\t')
        s++;
    return strlen(s) == 0;
}

/********************************************************************/

gchar *
expand_tilda(gchar *file)
{
    if (file[0] != '~')
        return g_strdup(file);

    const char * homedir = g_getenv("HOME");
    if (!homedir)
        homedir = g_get_home_dir();
    if (!homedir)
        homedir = g_get_tmp_dir();

    return g_strdup_printf("%s%s", homedir, file + 1);
}

/********************************************************************/

char* translate_exec_to_cmd( const char* exec, const char* icon,
                             const char* title, const char* fpath )
{
    GString* cmd = g_string_sized_new( 256 );
    for( ; *exec; ++exec )
    {
        if( G_UNLIKELY(*exec == '%') )
        {
            ++exec;
            if( !*exec )
                break;
            switch( *exec )
            {
                case 'c':
                    g_string_append( cmd, title );
                    break;
                case 'i':
                    if( icon )
                    {
                        g_string_append( cmd, "--icon " );
                        g_string_append( cmd, icon );
                    }
                    break;
                case 'k':
                {
                    char* uri = g_filename_to_uri( fpath, NULL, NULL );
                    g_string_append( cmd, uri );
                    g_free( uri );
                    break;
                }
                case '%':
                    g_string_append_c( cmd, '%' );
                    break;
            }
        }
        else
            g_string_append_c( cmd, *exec );
    }
    return g_string_free( cmd, FALSE );
}

/********************************************************************/

/*
 * Taken from pcmanfm:
 * Parse Exec command line of app desktop file, and translate
 * it into a real command which can be passed to g_spawn_command_line_async().
 * file_list is a null-terminated file list containing full
 * paths of the files passed to app.
 * returned char* should be freed when no longer needed.
 */
static char* translate_app_exec_to_command_line( const char* pexec,
                                                 GList* file_list )
{
    char* file;
    GList* l;
    gchar *tmp;
    GString* cmd = g_string_new("");
    gboolean add_files = FALSE;

    for( ; *pexec; ++pexec )
    {
        if( *pexec == '%' )
        {
            ++pexec;
            switch( *pexec )
            {
            case 'U':
                for( l = file_list; l; l = l->next )
                {
                    tmp = g_filename_to_uri( (char*)l->data, NULL, NULL );
                    file = g_shell_quote( tmp );
                    g_free( tmp );
                    g_string_append( cmd, file );
                    g_string_append_c( cmd, ' ' );
                    g_free( file );
                }
                add_files = TRUE;
                break;
            case 'u':
                if( file_list && file_list->data )
                {
                    file = (char*)file_list->data;
                    tmp = g_filename_to_uri( file, NULL, NULL );
                    file = g_shell_quote( tmp );
                    g_free( tmp );
                    g_string_append( cmd, file );
                    g_free( file );
                    add_files = TRUE;
                }
                break;
            case 'F':
            case 'N':
                for( l = file_list; l; l = l->next )
                {
                    file = (char*)l->data;
                    tmp = g_shell_quote( file );
                    g_string_append( cmd, tmp );
                    g_string_append_c( cmd, ' ' );
                    g_free( tmp );
                }
                add_files = TRUE;
                break;
            case 'f':
            case 'n':
                if( file_list && file_list->data )
                {
                    file = (char*)file_list->data;
                    tmp = g_shell_quote( file );
                    g_string_append( cmd, tmp );
                    g_free( tmp );
                    add_files = TRUE;
                }
                break;
            case 'D':
                for( l = file_list; l; l = l->next )
                {
                    tmp = g_path_get_dirname( (char*)l->data );
                    file = g_shell_quote( tmp );
                    g_free( tmp );
                    g_string_append( cmd, file );
                    g_string_append_c( cmd, ' ' );
                    g_free( file );
                }
                add_files = TRUE;
                break;
            case 'd':
                if( file_list && file_list->data )
                {
                    tmp = g_path_get_dirname( (char*)file_list->data );
                    file = g_shell_quote( tmp );
                    g_free( tmp );
                    g_string_append( cmd, file );
                    g_free( tmp );
                    add_files = TRUE;
                }
                break;
            case 'c':
                #if 0
                g_string_append( cmd, vfs_app_desktop_get_disp_name( app ) );
                #endif
                break;
            case 'i':
                /* Add icon name */
                #if 0
                if( vfs_app_desktop_get_icon_name( app ) )
                {
                    g_string_append( cmd, "--icon " );
                    g_string_append( cmd, vfs_app_desktop_get_icon_name( app ) );
                }
                #endif
                break;
            case 'k':
                /* Location of the desktop file */
                break;
            case 'v':
                /* Device name */
                break;
            case '%':
                g_string_append_c ( cmd, '%' );
                break;
            case '\0':
                goto _finish;
                break;
            }
        }
        else  /* not % escaped part */
        {
            g_string_append_c ( cmd, *pexec );
        }
    }
_finish:
    if( ! add_files )
    {
        g_string_append_c ( cmd, ' ' );
        for( l = file_list; l; l = l->next )
        {
            file = (char*)l->data;
            tmp = g_shell_quote( file );
            g_string_append( cmd, tmp );
            g_string_append_c( cmd, ' ' );
            g_free( tmp );
        }
    }

    gchar * result = g_string_free( cmd, FALSE );

    int len = strlen(result);
    while (len && result[len - 1] == ' ')
    {
        result[--len] = 0;
    }

    return result; 
}

gboolean lxpanel_launch(const char* command, GList* files)
{
    if (!command)
        return FALSE;

    while (*command == ' ' || *command == '\t')
        command++;

    int use_terminal = FALSE;

    if (*command == '&')
        use_terminal = TRUE,
        command++;

    if (!*command)
        return FALSE;

    return lxpanel_launch_app(command, files, use_terminal);
}

gboolean lxpanel_launch_app(const char* exec, GList* files, gboolean in_terminal)
{
    GError *error = NULL;
    char* cmd;
    if( ! exec )
        return FALSE;
    cmd = translate_app_exec_to_command_line(exec, files);
    if( in_terminal )
    {
        char* term_cmd;
        const char* term = lxpanel_get_terminal();
        if (!term)
        {
            g_free(cmd);
            return FALSE;
        }

	char * escaped_cmd = g_shell_quote(cmd);

        if( strstr(term, "%s") )
            term_cmd = g_strdup_printf(term, escaped_cmd);
        else
            term_cmd = g_strconcat( term, " -e ", escaped_cmd, NULL );
	g_free(escaped_cmd);
        if( cmd != exec )
            g_free(cmd);
        cmd = term_cmd;
    }

    //g_print("%s\n", cmd);

    if (! g_spawn_command_line_async(cmd, &error) ) {
        ERR("can't spawn %s\nError is %s\n", cmd, error->message);
        g_error_free (error);
    }
    g_free(cmd);

    return (error == NULL);
}

/********************************************************************/

gchar * panel_translate_directory_name(const gchar * name)
{
    gchar * title = NULL;

    if ( name )
    {
        /* load the name from *.directory file if needed */
        if ( g_str_has_suffix( name, ".directory" ) )
        {
            GKeyFile* kf = g_key_file_new();
            char* dir_file = g_build_filename( "desktop-directories", name, NULL );
            if ( g_key_file_load_from_data_dirs( kf, dir_file, NULL, 0, NULL ) )
            {
                title = g_key_file_get_locale_string( kf, "Desktop Entry", "Name", NULL, NULL );
            }
            g_free( dir_file );
            g_key_file_free( kf );
        }
    }

    if ( !title )
        title = g_strdup(name);

    return title;
}

/********************************************************************/

/* Open a specified path in a file manager. */
void lxpanel_open_in_file_manager(const char * path)
{
    char * quote = g_shell_quote(path);
    const char * fm = lxpanel_get_file_manager();
    char * cmd = ((strstr(fm, "%s") != NULL) ? g_strdup_printf(fm, quote) : g_strdup_printf("%s %s", fm, quote));
    g_free(quote);
    g_spawn_command_line_async(cmd, NULL);
    g_free(cmd);
}

/* Open a specified path in a terminal. */
void lxpanel_open_in_terminal(const char * path)
{
    const char * term = lxpanel_get_terminal();
    char * argv[2];
    char * sp = strchr(term, ' ');
    argv[0] = ((sp != NULL) ? g_strndup(term, sp - term) : (char *) term);
    argv[1] = NULL;
    g_spawn_async(path, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
    if (argv[0] != term)
        g_free(argv[0]);
}

/********************************************************************/

/* data used by themed images buttons */
typedef struct {
    char* fname;
    guint theme_changed_handler;
    GdkPixbuf* pixbuf;
    GdkPixbuf* hilight;
    gulong hicolor;
    int dw, dh; /* desired size */
    gboolean keep_ratio;
    gboolean use_dummy_image;
} ImgData;

static GQuark img_data_id = 0;

static void on_theme_changed(GtkIconTheme* theme, GtkWidget* img);
static void _gtk_image_set_from_file_scaled( GtkWidget* img, const gchar *file, gint width, gint height, gboolean keep_ratio, gboolean use_dummy_image);

/* DestroyNotify handler for image data in _gtk_image_new_from_file_scaled. */
static void img_data_free(ImgData * data)
{
    g_free(data->fname);
    if (data->theme_changed_handler != 0)
        g_signal_handler_disconnect(gtk_icon_theme_get_default(), data->theme_changed_handler);
    if (data->pixbuf != NULL)
        g_object_unref(data->pixbuf);
    if (data->hilight != NULL)
        g_object_unref(data->hilight);
    g_free(data);
}

/* Handler for "changed" signal in _gtk_image_new_from_file_scaled. */
static void on_theme_changed(GtkIconTheme * theme, GtkWidget * img)
{
    ImgData * data = (ImgData *) g_object_get_qdata(G_OBJECT(img), img_data_id);
    _gtk_image_set_from_file_scaled(img, data->fname, data->dw, data->dh, data->keep_ratio, data->use_dummy_image);
}

void fb_button_set_orientation(GtkWidget * btn, GtkOrientation orientation)
{
    GtkWidget * child = gtk_bin_get_child(GTK_BIN(btn));
    if (GTK_IS_BOX(child))
    {
        GtkBox *  newbox = GTK_BOX(recreate_box(GTK_BOX(child), orientation));
        if (GTK_WIDGET(newbox) != child)
        {
            gtk_container_add(GTK_CONTAINER(btn), GTK_WIDGET(newbox));
        }
    }
}

void fb_button_set_label(GtkWidget * btn, Panel * panel, gchar * label)
{
    /* Locate the label within the button. */
    GtkWidget * child = gtk_bin_get_child(GTK_BIN(btn));
    GtkWidget * lbl = NULL;
    if (GTK_IS_IMAGE(child))
    {
        /* No label. Create new. */

        GtkWidget * img = child;

        GtkWidget * inner = gtk_hbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(inner), 0);
        GTK_WIDGET_UNSET_FLAGS (inner, GTK_CAN_FOCUS);

        g_object_ref(G_OBJECT(img));
        gtk_container_remove(GTK_CONTAINER(btn), img);
        gtk_container_add(GTK_CONTAINER(btn), inner);

        gtk_box_pack_start(GTK_BOX(inner), img, FALSE, FALSE, 0);

        g_object_unref(G_OBJECT(img));

        lbl = gtk_label_new("");
        gtk_misc_set_padding(GTK_MISC(lbl), 2, 0);
        gtk_box_pack_start(GTK_BOX(inner), lbl, FALSE, FALSE, 0);

        gtk_widget_show_all(inner);
    }
    else if (GTK_IS_BOX(child))
    {
        GList * children = gtk_container_get_children(GTK_CONTAINER(child));
        if (children->next)
            lbl = GTK_WIDGET(GTK_IMAGE(children->next->data));
        g_list_free(children);
    }


    /* Update label text. */
    if (lbl)
        panel_draw_label_text(panel, lbl, label, FALSE, TRUE);
}

void fb_button_set_from_file(GtkWidget * btn, const char * img_file, gint width, gint height, gboolean keep_ratio)
{
    /* Locate the image within the button. */
    GtkWidget * child = gtk_bin_get_child(GTK_BIN(btn));
    GtkWidget * image = NULL;
    GtkWidget * label = NULL;

    if (GTK_IS_IMAGE(child))
    {
        image = child;
    }
    else if (GTK_IS_BOX(child))
    {
        GList * children = gtk_container_get_children(GTK_CONTAINER(child));
        image = GTK_WIDGET(GTK_IMAGE(children->data));
        if (children->next)
            label = GTK_WIDGET(GTK_IMAGE(children->next->data));
        g_list_free(children);
    }

    if (image)
    {
        ImgData * data = (ImgData *) g_object_get_qdata(G_OBJECT(image), img_data_id);
        g_free(data->fname);
        data->fname = g_strdup(img_file);
        data->dw = width;
        data->dh = height;
        data->keep_ratio = keep_ratio;
        data->use_dummy_image = label && !strempty(gtk_label_get_text(label));
        _gtk_image_set_from_file_scaled(image,
            data->fname, data->dw, data->dh, data->keep_ratio, data->use_dummy_image);

        gtk_widget_set_visible(image, !(strempty(img_file) && data->use_dummy_image));
    }
}

static void _gtk_image_set_from_file_scaled(GtkWidget * img, const gchar * file, gint width, gint height, gboolean keep_ratio, gboolean use_dummy_image)
{
    ImgData * data = (ImgData *) g_object_get_qdata(G_OBJECT(img), img_data_id);
    data->dw = width;
    data->dh = height;
    data->keep_ratio = keep_ratio;
    data->use_dummy_image = use_dummy_image;

    if (data->pixbuf != NULL)
    {
        g_object_unref(data->pixbuf);
        data->pixbuf = NULL;
    }

    /* if there is a cached hilighted version of this pixbuf, free it */
    if (data->hilight != NULL)
    {
        g_object_unref(data->hilight);
        data->hilight = NULL;
    }

    /* if they are the same string, eliminate unnecessary copy. */
    gboolean themed = FALSE;
    if (file != NULL && strlen(file) != 0)
    {
        if (data->fname != file)
        {
            g_free(data->fname);
            data->fname = g_strdup(file);
        }

        if (g_file_test(file, G_FILE_TEST_EXISTS))
        {
            GdkPixbuf * pb_scaled = _gdk_pixbuf_new_from_file_at_scale(file, width, height, keep_ratio);
            if (pb_scaled != NULL)
                data->pixbuf = pb_scaled;
        }
        else
        {
            data->pixbuf = lxpanel_load_icon(file, width, height, keep_ratio);
            themed = TRUE;
        }
    }

    if (data->pixbuf != NULL)
    {
        /* Set the pixbuf into the image widget. */
        gtk_image_set_from_pixbuf((GtkImage *)img, data->pixbuf);
        if (themed)
        {
            /* This image is loaded from icon theme.  Update the image if the icon theme is changed. */
            if (data->theme_changed_handler == 0)
                data->theme_changed_handler = g_signal_connect(gtk_icon_theme_get_default(), "changed", G_CALLBACK(on_theme_changed), img);
        }
        else
        {
            /* This is not loaded from icon theme.  Disconnect the signal handler. */
            if (data->theme_changed_handler != 0)
            {
                g_signal_handler_disconnect(gtk_icon_theme_get_default(), data->theme_changed_handler);
                data->theme_changed_handler = 0;
            }
        }
    }
    else
    {
        /* No pixbuf available.  Set the "missing image" icon. */
        if (data->use_dummy_image)
            gtk_image_set_from_stock(GTK_IMAGE(img), GTK_STOCK_MISSING_IMAGE, GTK_ICON_SIZE_BUTTON);
    }
    return;
}

GtkWidget * _gtk_image_new_from_file_scaled(const gchar * file, gint width, gint height, gboolean keep_ratio, gboolean use_dummy_image)
{
    GtkWidget * img = gtk_image_new();
    ImgData * data = g_new0(ImgData, 1);
    if (img_data_id == 0)
        img_data_id = g_quark_from_static_string("ImgData");
    g_object_set_qdata_full(G_OBJECT(img), img_data_id, data, (GDestroyNotify) img_data_free);
    _gtk_image_set_from_file_scaled(img, file, width, height, keep_ratio, use_dummy_image);

    gtk_misc_set_alignment(GTK_MISC(img), 0.5, 0.5);
    gtk_misc_set_padding (GTK_MISC(img), 0, 0);

    return img;
}


void
get_button_spacing(GtkRequisition *req, GtkContainer *parent, gchar *name)
{
    GtkWidget *b;
    //gint focus_width;
    //gint focus_pad;

    ENTER;
    b = gtk_button_new();
    gtk_widget_set_name(GTK_WIDGET(b), name);
    GTK_WIDGET_UNSET_FLAGS (b, GTK_CAN_FOCUS);
    GTK_WIDGET_UNSET_FLAGS (b, GTK_CAN_DEFAULT);
    gtk_container_set_border_width (GTK_CONTAINER (b), 0);

    if (parent)
        gtk_container_add(parent, b);

    gtk_widget_show(b);
    gtk_widget_size_request(b, req);

    gtk_widget_destroy(b);
    RET();
}


guint32 gcolor2rgb24(GdkColor *color)
{
    guint32 i;
    guint16 r, g, b;

    ENTER;

    r = color->red * 0xFF / 0xFFFF;
    g = color->green * 0xFF / 0xFFFF;
    b = color->blue * 0xFF / 0xFFFF;
    DBG("%x %x %x ==> %x %x %x\n", color->red, color->green, color->blue, r, g, b);

    i = r & 0xFF;
    i <<= 8;
    i |= g & 0xFF;
    i <<= 8;
    i |= b & 0xFF;
    DBG("i=%x\n", i);
    RET(i);
}

/* Handler for "enter-notify-event" signal on image that has highlighting requested. */
static gboolean fb_button_enter(GtkImage * widget, GdkEventCrossing * event)
{
    if (gtk_image_get_storage_type(widget) == GTK_IMAGE_PIXBUF)
    {
        ImgData * data = (ImgData *) g_object_get_qdata(G_OBJECT(widget), img_data_id);
        if (data != NULL)
        {
            if (data->hilight == NULL)
            {
                GdkPixbuf * dark = data->pixbuf;
                int height = gdk_pixbuf_get_height(dark);
                int rowstride = gdk_pixbuf_get_rowstride(dark);
                gulong hicolor = data->hicolor;

                GdkPixbuf * light = gdk_pixbuf_add_alpha(dark, FALSE, 0, 0, 0);
                if (light != NULL)
                {
                    guchar extra[3];
                    int i;
                    for (i = 2; i >= 0; i--, hicolor >>= 8)
                        extra[i] = hicolor & 0xFF;

                    guchar * src = gdk_pixbuf_get_pixels(light);
                    guchar * up;
                    for (up = src + height * rowstride; src < up; src += 4)
                    {
                        if (src[3] != 0)
                        {
                            for (i = 0; i < 3; i++)
                            {
                            int value = src[i] + extra[i];
                            if (value > 255) value = 255;
                            src[i] = value;
                            }
                        }
                    }
                data->hilight = light;
                }
            }

        if (data->hilight != NULL)
            gtk_image_set_from_pixbuf(widget, data->hilight);
        }
    }
    return TRUE;
}

/* Handler for "leave-notify-event" signal on image that has highlighting requested. */
static gboolean fb_button_leave(GtkImage * widget, GdkEventCrossing * event, gpointer user_data)
{
    if (gtk_image_get_storage_type(widget) == GTK_IMAGE_PIXBUF)
    {
        ImgData * data = (ImgData *) g_object_get_qdata(G_OBJECT(widget), img_data_id);
        if ((data != NULL) && (data->pixbuf != NULL))
            gtk_image_set_from_pixbuf(widget, data->pixbuf);
    }
    return TRUE;
}


GtkWidget * fb_button_new_from_file(
    gchar * image_file, int width, int height, gboolean highlighted, gboolean keep_ratio)
{
    return fb_button_new_from_file_with_label(image_file, width, height, highlighted, keep_ratio, NULL, NULL);
}

GtkWidget * fb_button_new_from_file_with_label(
    gchar * image_file, int width, int height, gboolean highlighted, gboolean keep_ratio, Panel * panel, gchar * label)
{
    gulong highlight_color = highlighted ? PANEL_ICON_HIGHLIGHT : 0;

    GtkWidget * event_box = gtk_event_box_new();
    gtk_container_set_border_width(GTK_CONTAINER(event_box), 0);
    gtk_widget_set_has_window(event_box, FALSE);
    GTK_WIDGET_UNSET_FLAGS(event_box, GTK_CAN_FOCUS);

    GtkWidget * image = _gtk_image_new_from_file_scaled(image_file, width, height, keep_ratio, !label || strlen(label) == 0);
    gtk_misc_set_padding(GTK_MISC(image), 0, 0);
    gtk_misc_set_alignment(GTK_MISC(image), 0, 0);
    if (highlight_color != 0)
    {
        ImgData * data = (ImgData *) g_object_get_qdata(G_OBJECT(image), img_data_id);
        data->hicolor = highlight_color;

        gtk_widget_add_events(event_box, GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
        g_signal_connect_swapped(G_OBJECT(event_box), "enter-notify-event", G_CALLBACK(fb_button_enter), image);
        g_signal_connect_swapped(G_OBJECT(event_box), "leave-notify-event", G_CALLBACK(fb_button_leave), image);
    }

    if (label == NULL)
        gtk_container_add(GTK_CONTAINER(event_box), image);
    else
    {
        GtkWidget * inner = gtk_hbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(inner), 0);
        GTK_WIDGET_UNSET_FLAGS (inner, GTK_CAN_FOCUS);
        gtk_container_add(GTK_CONTAINER(event_box), inner);

        gtk_box_pack_start(GTK_BOX(inner), image, FALSE, FALSE, 0);

        GtkWidget * lbl = gtk_label_new("");
        panel_draw_label_text(panel, lbl, label, FALSE, TRUE);
        gtk_misc_set_padding(GTK_MISC(lbl), 2, 0);
        gtk_box_pack_start(GTK_BOX(inner), lbl, FALSE, FALSE, 0);
    }

    gtk_misc_set_alignment(GTK_MISC(image), 0.5, 0.5);
    gtk_misc_set_padding (GTK_MISC(image), 0, 0);

    gtk_widget_show_all(event_box);

    if (panel)
        panel_require_update_background(panel);

    return event_box;
}

/********************************************************************/

/*
 This function is used to re-create a new box with different
 orientation from the old one, add all children of the old one to
 the new one, and then destroy the old box.
 It's mainly used when we need to change the orientation of the panel or
 any plugin with a layout box. Since GtkHBox cannot be changed to GtkVBox,
 recreating a new box to replace the old one is required.
*/
GtkWidget* recreate_box( GtkBox* oldbox, GtkOrientation orientation )
{
    GtkBox* newbox;
    GList *child, *children;
    GtkWidget* (*my_box_new) (gboolean homogeneous, gint spacing);

    if( GTK_IS_HBOX(oldbox) ) {
        if( orientation == GTK_ORIENTATION_HORIZONTAL )
            return GTK_WIDGET(oldbox);
    }
    else {
        if( orientation == GTK_ORIENTATION_VERTICAL )
            return GTK_WIDGET(oldbox);
    }
    my_box_new = (orientation == GTK_ORIENTATION_HORIZONTAL ? gtk_hbox_new : gtk_vbox_new);

    newbox = GTK_BOX(my_box_new( gtk_box_get_homogeneous(oldbox),
                                 gtk_box_get_spacing(oldbox) ));
    gtk_container_set_border_width (GTK_CONTAINER (newbox),
                                    gtk_container_get_border_width(GTK_CONTAINER(oldbox)) );
    children = gtk_container_get_children( GTK_CONTAINER (oldbox) );
    for( child = children; child; child = child->next ) {
        gboolean expand, fill;
        guint padding;
        GtkWidget* w = GTK_WIDGET(child->data);
        gtk_box_query_child_packing( oldbox, w,
                                     &expand, &fill, &padding, NULL );
        /* g_debug( "repack %s, expand=%d, fill=%d", gtk_widget_get_name(w), expand, fill ); */
        g_object_ref( w );
        gtk_container_remove( GTK_CONTAINER (oldbox), w );
        gtk_box_pack_start( newbox, w, expand, fill, padding );
        g_object_unref( w );
    }
    g_list_free( children );
    gtk_widget_show_all( GTK_WIDGET(newbox) );
    gtk_widget_destroy( GTK_WIDGET(oldbox) );
    return GTK_WIDGET(newbox);
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
        icon = _gdk_pixbuf_scale_in_rect(icon, width, height);
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

GdkPixbuf * lxpanel_load_icon(const char * name, int width, int height, gboolean use_fallback)
{
    GdkPixbuf * icon = NULL;

    if (name != NULL)
    {
        if (g_path_is_absolute(name))
        {
            /* Absolute path. */
            icon = _gdk_pixbuf_new_from_file_at_scale(name, width, height, TRUE);
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

void show_error( GtkWindow* parent_win, const char* msg )
{
    GtkWidget* dlg = gtk_message_dialog_new( parent_win,
                                             GTK_DIALOG_MODAL,
                                             GTK_MESSAGE_ERROR,
                                             GTK_BUTTONS_OK, "%s", msg );
    gtk_dialog_run( (GtkDialog*)dlg );
    gtk_widget_destroy( dlg );
}

/********************************************************************/

void bring_to_current_desktop(GtkWidget * win)
{
#if GTK_CHECK_VERSION(2,14,0)
    GdkWindow * gdk_window = gtk_widget_get_window(win);
    Window w = GDK_WINDOW_XID(gdk_window);
    int window_desktop = get_net_wm_desktop(w);
    int current_desktop = get_net_current_desktop();
    if (window_desktop != 0xFFFFFFFF && window_desktop != current_desktop)
    {
        set_net_wm_desktop(w, current_desktop);
    }
#endif
    gtk_window_present(GTK_WINDOW(win));
}

/********************************************************************/

static char * human_access (struct stat const *statbuf)
{
    static char modebuf[12];
    filemodestring (statbuf, modebuf);
    modebuf[10] = 0;
    return modebuf;
}


gchar * lxpanel_tooltip_for_file_stat(struct stat * stat_data)
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

void get_format_for_bytes_with_suffix(guint64  bytes, const char ** format, guint64 * b1, guint64 * b2)
{
    const char * f = NULL;
    if (bytes > 1 << 30)
    {
        bytes = (bytes * 10) / (1 << 30);
        f = "%lld.%lld GiB";
    }
    else if (bytes > 1 << 20)
    {
        bytes = (bytes * 10) / (1 << 20);
        f = "%lld.%lld MiB";
    }
    else if (bytes > 1 << 10)
    {
        bytes = (bytes * 10) / (1 << 10);
        f = "%lld.%lld KiB";
    }
    else if (bytes >= 0)
    {
        bytes = bytes * 10;
        f = "%lld B";
    }

    if (format)
        *format = f;
    if (b1)
        *b1 = bytes / 10;
    if (b2)
        *b2 = bytes % 10;
}

char * format_bytes_with_suffix(guint64  bytes)
{
    const char * format = NULL;
    guint64 b1;
    guint64 b2;

    get_format_for_bytes_with_suffix(bytes, &format, &b1, &b2);

    if (format)
        return g_strdup_printf (format, b1, b2);

    return NULL;
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

static char
ftypelet (mode_t bits)
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

void
strmode (mode_t mode, char *str)
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

void
filemodestring (struct stat const *statp, char *str)
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

