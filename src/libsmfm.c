/**
 * Copyright (c) 2012 Vadim Ushakov
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

#include <waterline/libsmfm.h>
#include <sde-utils.h>
#include <gtk/gtk.h>


static gboolean (*__fm_gtk_init)(void * config) = NULL;

static GtkMenu * (*__fm_get_gtk_file_menu_for_string)(GtkWindow* parent, const char * url) = NULL;

static gboolean libsmfm_initialized = FALSE;
static gboolean libsmfm_initialization_failed = FALSE;

gboolean wtl_fm_init(void)
{
    if (libsmfm_initialization_failed)
        return FALSE;

    if (libsmfm_initialized)
        return TRUE;

    GModule * libsmfm = g_module_open("libsmfm-gtk2.so.4", 0);
/*
    if (!libfm)
    {
        libfm = g_module_open("libfm-gtk.so.3", 0);
    }

    if (!libfm)
    {
        libfm = g_module_open("libfm-gtk.so.2", 0);
    }
*/
    if (!libsmfm)
    {
        su_print_error_message("Failed load libsmfm-gtk2.so.4");
        goto fail;
    }

#define bind_name(name) if (! g_module_symbol(libsmfm, #name, (gpointer * ) &__##name) ) \
{\
    su_print_error_message("Failed resolve %s\n", #name); \
    goto fail;\
}

    bind_name(fm_gtk_init);
    bind_name(fm_get_gtk_file_menu_for_string);

#undef bind_name

    __fm_gtk_init(NULL);

     libsmfm_initialized = TRUE;
     return TRUE;

fail:
     libsmfm_initialization_failed = TRUE;
     return FALSE;
}

GtkMenu * wtl_fm_file_menu_for_path(const char * path)
{
    if (!wtl_fm_init())
        return NULL;

    return __fm_get_gtk_file_menu_for_string(NULL, path);
}

