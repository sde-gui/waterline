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

/*
 * SuxPanel version 0.1
 * Copyright (c) 2003 Leandro Pereira <leandro@linuxmag.com.br>
 *
 * This program may be distributed under the terms of GNU General
 * Public License version 2. You should have received a copy of the
 * license with this program; if not, please consult http://www.fsf.org/.
 *
 * This program comes with no warranty. Use at your own risk.
 *
 */

#include <X11/Xatom.h>
#include <X11/cursorfont.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gio/gio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "misc.h"
#include "panel.h"

#include "dbg.h"

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
    ENTER;
    RET((file[0] == '~') ?
        g_strdup_printf("%s%s", getenv("HOME"), file+1)
        : g_strdup(file));

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

    return g_string_free( cmd, FALSE );
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
	char * escaped_cmd = g_shell_quote(cmd);
        char* term_cmd;
        const char* term = lxpanel_get_terminal();
        if( strstr(term, "%s") )
            term_cmd = g_strdup_printf(term, escaped_cmd);
        else
            term_cmd = g_strconcat( term, " -e ", escaped_cmd, NULL );
	g_free(escaped_cmd);
        if( cmd != exec )
            g_free(cmd);
        cmd = term_cmd;
    }
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
        GtkBox *  newbox = GTK_BOX(recreate_box(child, orientation));
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
    GtkWidget * img = NULL;
    if (GTK_IS_IMAGE(child))
        img = child;
    else if (GTK_IS_BOX(child))
    {
        GList * children = gtk_container_get_children(GTK_CONTAINER(child));
        img = GTK_WIDGET(GTK_IMAGE(children->data));
        g_list_free(children);
    }

    if (img != NULL)
    {
        ImgData * data = (ImgData *) g_object_get_qdata(G_OBJECT(img), img_data_id);
        g_free(data->fname);
        data->fname = g_strdup(img_file);
        data->dw = width;
        data->dh = height;
        data->keep_ratio = keep_ratio;
        data->use_dummy_image = TRUE;
        _gtk_image_set_from_file_scaled(img, data->fname, data->dw, data->dh, data->keep_ratio, data->use_dummy_image);
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
            GdkPixbuf * pb_scaled = gdk_pixbuf_new_from_file_at_scale(file, width, height, keep_ratio, NULL);
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

    i = (color->red * 0xFF / 0xFFFF) & 0xFF;
    i <<= 8;
    i |= (color->green * 0xFF / 0xFFFF) & 0xFF;
    i <<= 8;
    i |= (color->blue * 0xFF / 0xFFFF) & 0xFF;
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
    gchar * image_file, int width, int height, gulong highlight_color, gboolean keep_ratio)
{
    return fb_button_new_from_file_with_label(image_file, width, height, highlight_color, keep_ratio, NULL, NULL);
}

GtkWidget * fb_button_new_from_file_with_label(
    gchar * image_file, int width, int height, gulong highlight_color, gboolean keep_ratio, Panel * panel, gchar * label)
{
    GtkWidget * event_box = gtk_event_box_new();
    gtk_container_set_border_width(GTK_CONTAINER(event_box), 0);
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

/* Try to load an icon from a named file via the freedesktop.org data directories path.
 * http://standards.freedesktop.org/basedir-spec/basedir-spec-0.6.html */
static GdkPixbuf * load_icon_file(const char * file_name, int height, int width)
{
    GdkPixbuf * icon = NULL;
    const gchar ** dirs = (const gchar **) g_get_system_data_dirs();
    const gchar ** dir;
    for (dir = dirs; ((*dir != NULL) && (icon == NULL)); dir++)
    {
        char * file_path = g_build_filename(*dir, "pixmaps", file_name, NULL);
        icon = gdk_pixbuf_new_from_file_at_scale(file_path, height, width, TRUE, NULL);
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

    if (icon_name && strlen(icon_name) > 7 && memcmp("GIcon ", icon_name, 6) == 0)
    {
        GIcon * gicon = g_icon_new_for_string(icon_name + 6, NULL);
        icon_info = gtk_icon_theme_lookup_by_gicon(theme, gicon, width, GTK_ICON_LOOKUP_USE_BUILTIN);
        g_object_unref(G_OBJECT(gicon));
    }
    else
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
            icon = gdk_pixbuf_new_from_file_at_scale(name, width, height, TRUE, NULL);
        }
        else
        {
            /* Relative path. */
            GtkIconTheme * theme = gtk_icon_theme_get_default();
            char * suffix = strrchr(name, '.');
            if ((suffix != NULL)
            && ((g_strcasecmp(&suffix[1], "png") == 0)
              || (g_strcasecmp(&suffix[1], "svg") == 0)
              || (g_strcasecmp(&suffix[1], "xpm") == 0)))
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

