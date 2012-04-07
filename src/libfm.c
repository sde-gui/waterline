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

#define _LXPANEL_INTERNALS

#include "dbg.h"

#include <gtk/gtk.h>

#ifndef DISABLE_LIBFM
#include <libfm/fm-file-info.h>
#include <libfm/fm-file-menu.h>
#include <libfm/fm-config.h>
#endif


#ifndef DISABLE_LIBFM

static gboolean (*__fm_gtk_init)(FmConfig* config) = NULL;

static FmPath* (*__fm_path_new_for_path)(const char* path_name) = NULL;
static void (*__fm_path_unref)(FmPath* path) = NULL;

static FmFileInfo* (*__fm_file_info_new_from_gfileinfo)(FmPath* path, GFileInfo* inf) = NULL;
static void (*__fm_file_info_unref)( FmFileInfo* fi ) = NULL;

static FmFileMenu* (*__fm_file_menu_new_for_file)(GtkWindow* parent, FmFileInfo* fi, FmPath* cwd, gboolean auto_destroy) = NULL;
static GtkMenu* (*__fm_file_menu_get_menu)(FmFileMenu* menu) = NULL;

static gboolean libfm_initialized = FALSE;
static gboolean libfm_initialization_failed = FALSE;

gboolean lxpanel_fm_init(void)
{
    if (libfm_initialization_failed)
        return FALSE;

    if (libfm_initialized)
        return TRUE;

    GModule * libfm = g_module_open("libfm-gtk.so.2", 0);

    if (!libfm)
    {
        ERR("Failed load %s\n", "libfm-gtk.so.2");
        goto fail;
    }

#define bind_name(name) if (! g_module_symbol(libfm, #name, (gpointer * ) &__##name) ) \
{\
    ERR("Failed resolve %s\n", #name); \
    goto fail;\
}

    bind_name(fm_gtk_init);

    bind_name(fm_path_new_for_path);
    bind_name(fm_path_unref);

    bind_name(fm_file_info_new_from_gfileinfo);
    bind_name(fm_file_info_unref);

    bind_name(fm_file_menu_new_for_file);
    bind_name(fm_file_menu_get_menu);

#undef bind_name

    __fm_gtk_init(NULL);

     libfm_initialized = TRUE;
     return TRUE;

fail:
     libfm_initialization_failed = TRUE;
     return FALSE;
}

GtkMenu * lxpanel_fm_file_menu_for_path(const char * path)
{
    if (!path)
        return NULL;

    if (!lxpanel_fm_init())
        return NULL;

    GFile * gfile = NULL;
    GFileInfo * gfile_info = NULL;
    FmPath * fm_path = NULL;
    FmFileInfo * fm_file_info = NULL;
    GtkMenu * popup = NULL;

    gfile = g_file_new_for_path(path);
    if (!gfile)
        goto out;

    gfile_info = g_file_query_info(gfile, "standard::*,unix::*,time::*", G_FILE_QUERY_INFO_NONE, NULL, NULL);
    if (!gfile_info)
        goto out;
            
    fm_path = __fm_path_new_for_path(path);
    if (!fm_path)
        goto out;

    fm_file_info = __fm_file_info_new_from_gfileinfo(fm_path, gfile_info);
    if (!fm_file_info)
        goto out;

//    FmFileMenu * fm_file_menu = __fm_file_menu_new_for_file(GTK_WINDOW(p->panel->topgwin),
    FmFileMenu * fm_file_menu = __fm_file_menu_new_for_file(NULL,
                                                          fm_file_info,
                                                          /*cwd*/ NULL,
                                                          TRUE);
    if (!fm_file_menu)
        goto out;

    popup = __fm_file_menu_get_menu(fm_file_menu);
 
out:

    if (fm_file_info)
        __fm_file_info_unref(fm_file_info);
    if (fm_path)
        __fm_path_unref(fm_path);
    if (gfile_info)
        g_object_unref(G_OBJECT(gfile_info));
    if (gfile)
        g_object_unref(G_OBJECT(gfile));

    return popup;
}

#else

void lxpanel_fm_init(void) {}

GtkMenu * lxpanel_fm_file_menu_for_path(const char * path)
{
    return NULL;
}

#endif

