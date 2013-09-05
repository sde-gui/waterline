/**
 *
 * Copyright (c) 2011-2013 Vadim Ushakov
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

#include <waterline/dbg.h>

static gboolean initialized = FALSE;

static void (*_panel_configure)(Panel* p, int sel_page);
static void (*_configurator_remove_plugin_from_list)(Panel * p, Plugin * pl);

/* TODO: проверять, что основной бинарник и загружаемая библиотека компилировались вместе. */

static void load_implementation(void)
{
    if (initialized)
        return;

    initialized = TRUE;

    gchar * path = wtl_resolve_own_resource("lib", "internals", "libconfigurator.so", 0);
    if (!path)
        return;

    GModule * m = g_module_open(path, 0);
    if (!m)
    {
        ERR("%s: %s\n", path, g_module_error());
        goto err;
    }

    if (!g_module_symbol(m, "panel_configure", (gpointer*) &_panel_configure))
    {
        ERR("%s: symbol %s not found\n", path, "panel_configure");
    }

    if (!g_module_symbol(m, "configurator_remove_plugin_from_list", (gpointer*) &_configurator_remove_plugin_from_list))
    {
        ERR("%s: symbol %s not found\n", path, "configurator_remove_plugin_from_list");
    }

err:
    g_free(path);
}

void panel_configure(Panel* p, int sel_page)
{
    load_implementation();
    if (_panel_configure)
        _panel_configure(p, sel_page);
}

void configurator_remove_plugin_from_list(Panel * p, Plugin * pl)
{
    load_implementation();
    if (_configurator_remove_plugin_from_list)
        _configurator_remove_plugin_from_list(p, pl);
}

