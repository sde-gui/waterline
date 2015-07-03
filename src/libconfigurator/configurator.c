/**
 *
 * Copyright (c) 2011-2015 Vadim Ushakov
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
#include "config.h"
#endif

#include <waterline/global.h>
#include "plugin_internal.h"
#include "plugin_private.h"
#include <waterline/panel.h>
#include "panel_internal.h"
#include "panel_private.h"
#include <waterline/paths.h>
#include <waterline/misc.h>
#include <waterline/defaultapplications.h>
#include "bg.h"
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <glib/gi18n.h>

enum{
    COL_NAME,
    COL_EXPAND,
    COL_DATA,
    N_COLS
};

static void update_opt_menu(GtkWidget *w, int ind);

/******************************************************************************/

/* defined in  generic_config_dlg.c */

extern gboolean on_entry_focus_out( GtkWidget* edit, GdkEventFocus *evt, gpointer user_data );

/* defined in  configurator_plugin_list.c */

extern void initialize_plugin_list(Panel * p, GtkBuilder * builder);
extern void initialize_background_controls(Panel * p, GtkBuilder * builder);

/******************************************************************************/

static void gui_update_width(Panel* p)
{
    p->pref_dialog.doing_update++;

    GtkSpinButton * spin = GTK_SPIN_BUTTON(p->pref_dialog.width_control);

    gtk_widget_set_sensitive(GTK_WIDGET(spin), p->oriented_width_type!= WIDTH_REQUEST);

    if (p->oriented_width_type == WIDTH_PERCENT)
    {
        gtk_spin_button_set_range(spin, 0, 100);
    }
    else if (p->oriented_width_type == WIDTH_PIXEL)
    {
        if ((p->edge == EDGE_TOP) || (p->edge == EDGE_BOTTOM))
        {
            gtk_spin_button_set_range(spin, 0, p->output_target_width);
        }
        else
        {
            gtk_spin_button_set_range(spin, 0, p->output_target_height);
        }
    }

    gtk_spin_button_set_value(spin, p->oriented_width);

    gtk_combo_box_set_active(GTK_COMBO_BOX(p->pref_dialog.width_unit), p->oriented_width_type - 1);

    p->pref_dialog.doing_update--;
}

static void gui_update_visibility(Panel* p)
{
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p->pref_dialog.always_visible),
        (p->visibility_mode == VISIBILITY_ALWAYS));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p->pref_dialog.always_below),
        (p->visibility_mode == VISIBILITY_BELOW));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p->pref_dialog.autohide),
        (p->visibility_mode == VISIBILITY_AUTOHIDE));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p->pref_dialog.gobelow),
        (p->visibility_mode == VISIBILITY_GOBELOW));

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(p->pref_dialog.height_when_minimized),
        p->height_when_hidden);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p->pref_dialog.reserve_space),
        p->set_strut);

    gtk_widget_set_sensitive(p->pref_dialog.height_when_minimized,
        (p->visibility_mode == VISIBILITY_AUTOHIDE) || (p->visibility_mode == VISIBILITY_GOBELOW));

    gtk_widget_set_sensitive(p->pref_dialog.reserve_space,
        (p->visibility_mode != VISIBILITY_AUTOHIDE) && (p->visibility_mode != VISIBILITY_GOBELOW));

    gtk_widget_set_sensitive(p->pref_dialog.edge_margin_control,
        (p->visibility_mode != VISIBILITY_AUTOHIDE) && (p->visibility_mode != VISIBILITY_GOBELOW));
}

/******************************************************************************/

static void
response_event(GtkDialog *widget, gint arg1, Panel* panel )
{
    switch (arg1) {
    /* FIXME: what will happen if the user exit the program without
              close this config dialog?
              Then the config won't be save, I guess. */
    case GTK_RESPONSE_DELETE_EVENT:
    case GTK_RESPONSE_CLOSE:
    case GTK_RESPONSE_NONE:
        panel_save_configuration(panel);
        /* NOTE: NO BREAK HERE*/
        gtk_widget_destroy(GTK_WIDGET(widget));
        break;
    }
    return;
}

static void set_edge(Panel* p, int edge)
{
    if (p->edge == edge)
        return;
    p->edge = edge;
    panel_update_geometry(p);
    panel_set_panel_configuration_changed(p);
    panel_adjust_geometry_terminology(p);
}

static void edge_changed(GtkWidget *widget, Panel *p)
{
    int edge = -1;
    int i = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
    switch (i)
    {
        case 0: edge = EDGE_TOP; break;
        case 1: edge = EDGE_BOTTOM; break;
        case 2: edge = EDGE_LEFT; break;
        case 3: edge = EDGE_RIGHT; break;
    }
    if (edge != -1)
        set_edge(p, edge);
}

static void
set_edge_margin( GtkSpinButton* spin,  Panel* p  )
{
    int edge_margin = gtk_spin_button_get_value(spin);
    if (p->edge_margin == edge_margin)
        return;
    p->edge_margin = edge_margin;
    panel_update_geometry(p);
}

static void set_alignment(Panel* p, int align)
{
    if (p->pref_dialog.align_margin_control)
        gtk_widget_set_sensitive(p->pref_dialog.align_margin_control, (align != ALIGN_CENTER));
    p->align = align;
    panel_update_geometry(p);
    panel_set_panel_configuration_changed(p);
    panel_adjust_geometry_terminology(p);
}

static void align_left_toggle(GtkToggleButton *widget, Panel *p)
{
    if (gtk_toggle_button_get_active(widget))
        set_alignment(p, ALIGN_LEFT);
}

static void align_center_toggle(GtkToggleButton *widget, Panel *p)
{
    if (gtk_toggle_button_get_active(widget))
        set_alignment(p, ALIGN_CENTER);
}

static void align_right_toggle(GtkToggleButton *widget, Panel *p)
{
    if (gtk_toggle_button_get_active(widget))
        set_alignment(p, ALIGN_RIGHT);
}

static void
set_align_margin( GtkSpinButton* spin,  Panel* p  )
{
    int align_margin = gtk_spin_button_get_value(spin);
    if (p->align_margin == align_margin)
        return;
    p->align_margin = align_margin;
    panel_update_geometry(p);
}

static void
set_width(  GtkSpinButton* spin, Panel* p )
{
    if (p->pref_dialog.doing_update)
        return;

    int oriented_width = gtk_spin_button_get_value(spin);
    if (p->oriented_width == oriented_width)
        return;

    p->oriented_width = oriented_width;
    panel_update_geometry(p);
    panel_set_panel_configuration_changed(p);
}

static void
set_height( GtkSpinButton* spin, Panel* p )
{
    p->oriented_height = (int)gtk_spin_button_get_value(spin);
    panel_update_geometry(p);
    panel_set_panel_configuration_changed(p);
}

static void set_width_type( GtkWidget *item, Panel* p )
{
    if (p->pref_dialog.doing_update)
        return;

    int widthtype = gtk_combo_box_get_active(GTK_COMBO_BOX(item)) + 1;

    if (p->oriented_width_type == widthtype)
        return;

    if (widthtype == WIDTH_PERCENT || widthtype == WIDTH_PIXEL)
    {
        int max_width = ((p->edge == EDGE_TOP) || (p->edge == EDGE_BOTTOM)) ?
            p->output_target_width :
            p->output_target_height;
        int width = p->oriented_width;

        if (widthtype == WIDTH_PERCENT)
        {
            if (p->oriented_width_type == WIDTH_PIXEL)
                width = 100 * width / max_width;
            else
                width = 100;
        }
        else if (widthtype == WIDTH_PIXEL)
        {
            if (p->oriented_width_type == WIDTH_PERCENT)
                width = (width + 1) * max_width / 100;
            else
                width = max_width;
        }
        p->oriented_width = width;
    }

    p->oriented_width_type = widthtype;

    panel_update_geometry(p);
    panel_set_panel_configuration_changed(p);

    gui_update_width(p);
}


static void set_visibility(Panel* p, int visibility_mode)
{
    if (p->visibility_mode == visibility_mode)
        return;
    p->visibility_mode = visibility_mode;
    panel_update_geometry(p);
    gui_update_visibility(p);
}

static void always_visible_toggle(GtkToggleButton *widget, Panel *p)
{
    if (gtk_toggle_button_get_active(widget))
        set_visibility(p, VISIBILITY_ALWAYS);
}

static void always_below_toggle(GtkToggleButton *widget, Panel *p)
{
    if (gtk_toggle_button_get_active(widget))
        set_visibility(p, VISIBILITY_BELOW);
}

static void autohide_toggle(GtkToggleButton *widget, Panel *p)
{
    if (gtk_toggle_button_get_active(widget))
        set_visibility(p, VISIBILITY_AUTOHIDE);
}

static void gobelow_toggle(GtkToggleButton *widget, Panel *p)
{
    if (gtk_toggle_button_get_active(widget))
        set_visibility(p, VISIBILITY_GOBELOW);
}

static void
on_font_color_set( GtkColorButton* clr,  Panel* p )
{
    gtk_color_button_get_color( clr, &p->font_color );
    panel_set_panel_configuration_changed(p);
}

static void
on_use_font_color_toggled( GtkToggleButton* btn,   Panel* p )
{
    GtkWidget* clr = (GtkWidget*)g_object_get_data( G_OBJECT(btn), "clr" );
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn)))
        gtk_widget_set_sensitive( clr, TRUE );
    else
        gtk_widget_set_sensitive( clr, FALSE );
    p->use_font_color = gtk_toggle_button_get_active( btn );
    panel_set_panel_configuration_changed(p);
}

static void
on_font_size_set( GtkSpinButton* spin, Panel* p )
{
    p->font_size = (int)gtk_spin_button_get_value(spin);
    panel_set_panel_configuration_changed(p);
}

static void
on_use_font_size_toggled( GtkToggleButton* btn,   Panel* p )
{
    GtkWidget* clr = (GtkWidget*)g_object_get_data( G_OBJECT(btn), "clr" );
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn)))
        gtk_widget_set_sensitive( clr, TRUE );
    else
        gtk_widget_set_sensitive( clr, FALSE );
    p->use_font_size = gtk_toggle_button_get_active( btn );
    panel_set_panel_configuration_changed(p);
}

static void
set_strut(GtkToggleButton* toggle,  Panel* p )
{
    p->set_strut = gtk_toggle_button_get_active(toggle) ? 1 : 0;
    panel_update_geometry(p);
}

static void
set_height_when_minimized( GtkSpinButton* spin,  Panel* p  )
{
    p->height_when_hidden = (int)gtk_spin_button_get_value(spin);
    panel_update_geometry(p);
}

static void
set_icon_size( GtkSpinButton* spin,  Panel* p  )
{
    p->preferred_icon_size = (int)gtk_spin_button_get_value(spin);
    panel_set_panel_configuration_changed(p);
}

static void on_output_target_changed(GtkWidget * item, Panel * p)
{
    int output_target = gtk_combo_box_get_active(GTK_COMBO_BOX(item));
    if (p->output_target != output_target)
    {
        p->output_target = output_target;
        panel_update_geometry(p);
        gui_update_width(p);

        gtk_widget_set_sensitive(GTK_WIDGET(p->pref_dialog.custom_monitor), p->output_target == OUTPUT_CUSTOM_MONITOR);
    }
}

static void
on_custom_monitor_value_changed( GtkSpinButton* spin,  Panel* p  )
{
    int custom_monitor = gtk_spin_button_get_value(spin);
    if (p->custom_monitor != custom_monitor)
    {
        p->custom_monitor = custom_monitor;
        panel_update_geometry(p);
        gui_update_width(p);
    }
}


static void
on_paddings_value_changed(GtkSpinButton* spin, Panel* p)
{
    p->padding_top = gtk_spin_button_get_value(p->pref_dialog.padding_top);
    p->padding_bottom = gtk_spin_button_get_value(p->pref_dialog.padding_bottom);
    p->padding_left = gtk_spin_button_get_value(p->pref_dialog.padding_left);
    p->padding_right = gtk_spin_button_get_value(p->pref_dialog.padding_right);
    p->applet_spacing = gtk_spin_button_get_value(p->pref_dialog.applet_spacing);
    panel_set_panel_configuration_changed(p);
}

static void
update_preferred_applications_info_label(Panel * p)
{
    gtk_label_set_markup (GTK_LABEL(p->pref_dialog.preferred_applications_info_label), "");

    gchar * info_text_0 = g_strdup_printf("%s",
        _("<i>If you leave these fields empty,\n"
          "Waterline will try to automatically detect the appropriate applications.</i>"));

    gchar * info_text_1 = NULL;
    if (!su_str_empty(gtk_entry_get_text(GTK_ENTRY(p->pref_dialog.preferred_applications_file_manager)))) {
        info_text_1 = g_strdup("");
    }
    else if (!su_str_empty(wtl_get_default_application("file-manager"))) {
        info_text_1 = g_strdup_printf(
            _("\n\n<i>Automatically detected <b>file manager</b>:</i> %s"),
            wtl_get_default_application("file-manager"));
    }
    else {
        info_text_1 = g_strdup_printf("%s",
            _("\n\n<span foreground='red'><b>WARNING:</b></span> "
             "<i>Waterline failed to automatically detect your <b>file manager</b>.\n"
             "Please specify the preferable <b>file manager</b> in the field above.</i>"));
    }

    gchar * info_text_2 = NULL;
    if (!su_str_empty(gtk_entry_get_text(GTK_ENTRY(p->pref_dialog.preferred_applications_terminal_emulator)))) {
        info_text_2 = g_strdup("");
    }
    else if (!su_str_empty(wtl_get_default_application("terminal-emulator"))) {
        info_text_2 = g_strdup_printf(
            _("\n\n<i>Automatically detected <b>terminal emulator</b>:</i> %s"),
            wtl_get_default_application("terminal-emulator"));
    }
    else {
        info_text_2 = g_strdup_printf("%s",
            _("\n\n<span foreground='red'><b>WARNING:</b></span> "
              "<i>Waterline failed to automatically detect your <b>terminal emulator</b>.\n"
              "Please specify the preferable <b>terminal emulator</b> in the field above.</i>"));
    }

    gchar * info_text_3 = NULL;
    if (!su_str_empty(gtk_entry_get_text(GTK_ENTRY(p->pref_dialog.preferred_applications_logout)))) {
        info_text_3 = g_strdup("");
    }
    else if (!su_str_empty(wtl_get_default_application("logout"))) {
        info_text_3 = g_strdup_printf(
            _("\n\n<i>Automatically detected <b>logout command</b>:</i> %s"),
            wtl_get_default_application("logout"));
    }
    else {
        info_text_3 = g_strdup_printf("%s",
            _("\n\n<span foreground='red'><b>WARNING:</b></span> "
              "<i>Waterline failed to automatically detect your <b>logout command</b>.\n"
              "Please specify the preferable <b>logout command</b> in the field above.</i>"));
    }

    gchar * info_text = g_strconcat(info_text_0, info_text_1, info_text_2, info_text_3, NULL);

    gtk_label_set_markup (GTK_LABEL(p->pref_dialog.preferred_applications_info_label), info_text);

    g_free(info_text);
    g_free(info_text_0);
    g_free(info_text_1);
    g_free(info_text_2);
    g_free(info_text_3);
}


static void
on_preferred_applications_changed(GtkEditable * editable, Panel * p)
{
    update_preferred_applications_info_label(p);
}

static void
update_opt_menu(GtkWidget *w, int ind)
{
    int i;

    /* this trick will trigger "changed" signal even if active entry is
     * not actually changing */
    i = gtk_combo_box_get_active(GTK_COMBO_BOX(w));
    if (i == ind) {
        i = i ? 0 : 1;
        gtk_combo_box_set_active(GTK_COMBO_BOX(w), i);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(w), ind);
}

static GtkSpinButton * connect_spinbutton(GtkBuilder * builder, const char * name, int value, GCallback value_changed, void * p)
{
    GtkSpinButton * w = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, name));
    if (!w)
        return NULL;

    gtk_spin_button_set_value(w, value);

    if (value_changed)
        g_signal_connect(w, "value-changed", value_changed, p);

    return w;
}

#define CONNECT_SPINBUTTON(name, value_changed) \
    p->pref_dialog.name = connect_spinbutton(builder, #name, p->name, G_CALLBACK(value_changed), p);


static
void panel_initialize_pref_dialog(Panel * p)
{
    GtkBuilder * builder;
    GtkWidget *w, *w2;

    gchar * panel_perf_ui_path = wtl_resolve_own_resource("", "ui", "panel-pref.ui", 0);
    builder = gtk_builder_new();
    if( !gtk_builder_add_from_file(builder, panel_perf_ui_path, NULL) )
    {
        g_free(panel_perf_ui_path);
        g_object_unref(builder);
        return;
    }

    g_free(panel_perf_ui_path);

    p->pref_dialog.pref_dialog = (GtkWidget*)gtk_builder_get_object( builder, "panel_pref" );
    g_signal_connect(p->pref_dialog.pref_dialog, "response", (GCallback) response_event, p);
    g_object_add_weak_pointer( G_OBJECT(p->pref_dialog.pref_dialog), (gpointer) &p->pref_dialog.pref_dialog );
    gtk_window_set_position( (GtkWindow*)p->pref_dialog.pref_dialog, GTK_WIN_POS_CENTER );
    panel_apply_icon(GTK_WINDOW(p->pref_dialog.pref_dialog));

    /* position */

    /* edge */

    w = (GtkWidget*)gtk_builder_get_object( builder, "edge" );
    gtk_widget_set_sensitive(w, TRUE);
    int edge_index = 0;
    switch (p->edge)
    {
        case EDGE_TOP   : edge_index = 0; break;
        case EDGE_BOTTOM: edge_index = 1; break;
        case EDGE_LEFT  : edge_index = 2; break;
        case EDGE_RIGHT : edge_index = 3; break;
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(w), edge_index);
    g_signal_connect( w, "changed", G_CALLBACK(edge_changed), p);

    /* alignment */
    p->pref_dialog.alignment_left_label = w = (GtkWidget*)gtk_builder_get_object( builder, "alignment_left" );
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), (p->align == ALIGN_LEFT));
    g_signal_connect(w, "toggled", G_CALLBACK(align_left_toggle), p);
    w = (GtkWidget*)gtk_builder_get_object( builder, "alignment_center" );
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), (p->align == ALIGN_CENTER));
    g_signal_connect(w, "toggled", G_CALLBACK(align_center_toggle), p);
    p->pref_dialog.alignment_right_label = w = (GtkWidget*)gtk_builder_get_object( builder, "alignment_right" );
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), (p->align == ALIGN_RIGHT));
    g_signal_connect(w, "toggled", G_CALLBACK(align_right_toggle), p);

    /* edge margin */
    p->pref_dialog.edge_margin_label = (GtkWidget*)gtk_builder_get_object( builder, "edge_margin_label");
    p->pref_dialog.edge_margin_control = w = (GtkWidget*)gtk_builder_get_object( builder, "edge_margin" );
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), p->edge_margin);
    g_signal_connect( w, "value-changed",
                      G_CALLBACK(set_edge_margin), p);

    /* align margin */
    p->pref_dialog.align_margin_label = (GtkWidget*)gtk_builder_get_object( builder, "align_margin_label");
    p->pref_dialog.align_margin_control = w = (GtkWidget*)gtk_builder_get_object( builder, "align_margin" );
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), p->align_margin);
    gtk_widget_set_sensitive(p->pref_dialog.align_margin_control, (p->align != ALIGN_CENTER));
    g_signal_connect( w, "value-changed",
                      G_CALLBACK(set_align_margin), p);

    /* width */
    p->pref_dialog.width_label = (GtkWidget*)gtk_builder_get_object( builder, "width_label");
    p->pref_dialog.width_control = w = (GtkWidget*)gtk_builder_get_object( builder, "width" );
    g_signal_connect( w, "value-changed", G_CALLBACK(set_width), p );

    p->pref_dialog.width_unit = w = (GtkWidget*)gtk_builder_get_object( builder, "width_unit" );
    g_signal_connect( w, "changed", G_CALLBACK(set_width_type), p);

    gui_update_width(p);

    /* height */

    p->pref_dialog.height_label = (GtkWidget*)gtk_builder_get_object( builder, "height_label");
    p->pref_dialog.height_control = w = (GtkWidget*)gtk_builder_get_object( builder, "height" );
    gtk_spin_button_set_range( (GtkSpinButton*)w, PANEL_HEIGHT_MIN, PANEL_HEIGHT_MAX );
    gtk_spin_button_set_value( (GtkSpinButton*)w, p->oriented_height );
    g_signal_connect( w, "value-changed", G_CALLBACK(set_height), p );

    w = (GtkWidget*)gtk_builder_get_object( builder, "height_unit" );
    update_opt_menu( w, HEIGHT_PIXEL - 1);

    w = (GtkWidget*)gtk_builder_get_object( builder, "icon_size" );
    gtk_spin_button_set_range( (GtkSpinButton*)w, PANEL_HEIGHT_MIN, PANEL_HEIGHT_MAX );
    gtk_spin_button_set_value( (GtkSpinButton*)w, p->preferred_icon_size );
    g_signal_connect( w, "value_changed", G_CALLBACK(set_icon_size), p );

    /* visibility */

    p->pref_dialog.always_visible = w = (GtkWidget*)gtk_builder_get_object( builder, "always_visible" );
    g_signal_connect(w, "toggled", G_CALLBACK(always_visible_toggle), p);

    p->pref_dialog.always_below = w = (GtkWidget*)gtk_builder_get_object( builder, "always_below" );
    g_signal_connect(w, "toggled", G_CALLBACK(always_below_toggle), p);

    p->pref_dialog.autohide = w = (GtkWidget*)gtk_builder_get_object( builder, "autohide" );
    g_signal_connect(w, "toggled", G_CALLBACK(autohide_toggle), p);

    p->pref_dialog.gobelow = w = (GtkWidget*)gtk_builder_get_object( builder, "gobelow" );
    g_signal_connect(w, "toggled", G_CALLBACK(gobelow_toggle), p);

    p->pref_dialog.reserve_space = w = (GtkWidget*)gtk_builder_get_object( builder, "reserve_space" );
    g_signal_connect( w, "toggled",
                      G_CALLBACK(set_strut), p );

    p->pref_dialog.height_when_minimized = w = (GtkWidget*)gtk_builder_get_object( builder, "height_when_minimized" );
    g_signal_connect( w, "value-changed",
                      G_CALLBACK(set_height_when_minimized), p);

    gui_update_visibility(p);

    /* monitor */

    w = (GtkWidget *) gtk_builder_get_object( builder, "output_target" );
    gtk_combo_box_set_active(GTK_COMBO_BOX(w), p->output_target);
    g_signal_connect(w, "changed", G_CALLBACK(on_output_target_changed), p);

    p->pref_dialog.custom_monitor = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "custom_monitor"));
    gtk_spin_button_set_range(p->pref_dialog.custom_monitor, 0, gdk_screen_get_n_monitors(gtk_widget_get_screen(p->topgwin)) - 1);
    gtk_spin_button_set_value(p->pref_dialog.custom_monitor, p->custom_monitor);
    gtk_widget_set_sensitive(GTK_WIDGET(p->pref_dialog.custom_monitor), p->output_target == OUTPUT_CUSTOM_MONITOR);
    g_signal_connect(p->pref_dialog.custom_monitor, "value_changed", G_CALLBACK(on_custom_monitor_value_changed), p);

    initialize_background_controls(p, builder);

    /* font color */
    w = (GtkWidget*)gtk_builder_get_object( builder, "font_clr" );
    gtk_color_button_set_color( (GtkColorButton*)w, &p->font_color );
    g_signal_connect( w, "color-set", G_CALLBACK( on_font_color_set ), p );

    w2 = (GtkWidget*)gtk_builder_get_object( builder, "use_font_clr" );
    gtk_toggle_button_set_active( (GtkToggleButton*)w2, p->use_font_color );
    g_object_set_data( G_OBJECT(w2), "clr", w );
    g_signal_connect(w2, "toggled", G_CALLBACK(on_use_font_color_toggled), p);
    if( ! p->use_font_color )
        gtk_widget_set_sensitive( w, FALSE );

    /* font size */
    w = (GtkWidget*)gtk_builder_get_object( builder, "font_size" );
    gtk_spin_button_set_value( (GtkSpinButton*)w, p->font_size );
    g_signal_connect( w, "value-changed",
                      G_CALLBACK(on_font_size_set), p);

    w2 = (GtkWidget*)gtk_builder_get_object( builder, "use_font_size" );
    gtk_toggle_button_set_active( (GtkToggleButton*)w2, p->use_font_size );
    g_object_set_data( G_OBJECT(w2), "clr", w );
    g_signal_connect(w2, "toggled", G_CALLBACK(on_use_font_size_toggled), p);
    if( ! p->use_font_size )
        gtk_widget_set_sensitive( w, FALSE );

    CONNECT_SPINBUTTON(padding_top, on_paddings_value_changed);
    CONNECT_SPINBUTTON(padding_bottom, on_paddings_value_changed);
    CONNECT_SPINBUTTON(padding_left, on_paddings_value_changed);
    CONNECT_SPINBUTTON(padding_right, on_paddings_value_changed);
    CONNECT_SPINBUTTON(applet_spacing, on_paddings_value_changed);

    /* plugin list */
    initialize_plugin_list(p, builder);

    /* advanced, applications */
    p->pref_dialog.preferred_applications_file_manager = w = (GtkWidget*)gtk_builder_get_object( builder, "file_manager" );
    if (global_config.file_manager_cmd)
        gtk_entry_set_text( (GtkEntry*)w, global_config.file_manager_cmd );
    g_signal_connect(w, "focus-out-event", G_CALLBACK(on_entry_focus_out), &global_config.file_manager_cmd);
    g_signal_connect(w, "changed", G_CALLBACK(on_preferred_applications_changed), p);

    p->pref_dialog.preferred_applications_terminal_emulator = w = (GtkWidget*)gtk_builder_get_object( builder, "term" );
    if (global_config.terminal_cmd)
        gtk_entry_set_text( (GtkEntry*)w, global_config.terminal_cmd );
    g_signal_connect(w, "focus-out-event", G_CALLBACK(on_entry_focus_out), &global_config.terminal_cmd);
    g_signal_connect(w, "changed", G_CALLBACK(on_preferred_applications_changed), p);

    /* If we are under LXSession, setting logout command is not necessary. */
    p->pref_dialog.preferred_applications_logout = w = (GtkWidget*)gtk_builder_get_object( builder, "logout" );
    if(global_config.logout_cmd)
        gtk_entry_set_text( (GtkEntry*)w, global_config.logout_cmd );
    g_signal_connect(w, "focus-out-event", G_CALLBACK(on_entry_focus_out), &global_config.logout_cmd);
    g_signal_connect(w, "changed", G_CALLBACK(on_preferred_applications_changed), p);

    p->pref_dialog.preferred_applications_info_label = (GtkWidget *) gtk_builder_get_object(builder,
        "preferred_applications_info_label");

    update_preferred_applications_info_label(p);

    p->pref_dialog.notebook = (GtkWidget*)gtk_builder_get_object( builder, "notebook" );

    g_object_unref(builder);
}


void panel_configure( Panel* p, int sel_page )
{
    if (!p->pref_dialog.pref_dialog)
    {
        panel_initialize_pref_dialog(p);
    }

    if (!p->pref_dialog.pref_dialog)
        return;

    panel_adjust_geometry_terminology(p);
    gtk_widget_show(GTK_WIDGET(p->pref_dialog.pref_dialog));
    wtl_util_bring_window_to_current_desktop(p->pref_dialog.pref_dialog);

    if (sel_page >= 0)
        gtk_notebook_set_current_page(GTK_NOTEBOOK(p->pref_dialog.notebook), sel_page);
}

