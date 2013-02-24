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

#include <lxpanelx/misc.h>
#include <glib.h>

static char * de_name = NULL;
static gboolean initialized = FALSE;

const char * get_de_name(void)
{
    if (initialized)
        return de_name;

    const char * name = g_getenv("XDG_CURRENT_DESKTOP");
    if (!name)
        name = g_getenv("DESKTOP_SESSION");

    if (name)
        de_name = g_ascii_strup(name, -1);
    initialized = TRUE;

    return de_name;
}
