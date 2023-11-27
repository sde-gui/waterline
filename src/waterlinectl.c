/**
 * Copyright (c) 2013 Vadim Ushakov
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

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Display * dpy;

static const char usage[] =
    "\nwaterlinectl - Waterline Panel Controller\n"
    "Usage: waterlinectl <command>\n\n"
    "Available commands:\n"
    "menu\tshow system menu\n"
    "run\tshow run dialog\n"
    "config\tshow configuration dialog\n"
    "restart\trestart waterline\n"
    "exit\texit waterline\n\n";

int main(int argc, const char** argv)
{
    const char * display_name = (char *) getenv("DISPLAY");
    Window root;
    Atom cmd_atom;

    if( argc < 2 )
    {
        printf( usage );
        return 1;
    }

    {
        size_t buff_size = 0;
        int i;
        for (i = 1; i < argc; i++)
        {
            buff_size += strlen(argv[i]) + 1;
        }

        char * buff = (char *) calloc(buff_size, sizeof(char));
        if (buff == NULL) {
            errx(1, "memory allocation failure (%zu bytes)", buff_size);
            return 1;
        }

        size_t buff_pos = 0;
        for (i = 1; i < argc; i++)
        {
            size_t s = strlen(argv[i]) + 1;
            memcpy(buff + buff_pos, argv[i], s * sizeof(char));
            buff_pos += s;
        }

        dpy = XOpenDisplay(display_name);
        if (dpy == NULL) {
            err(1, "cannot open display: %s", display_name);
            return 1;
        }
        root = DefaultRootWindow(dpy);
        cmd_atom = XInternAtom(dpy, "_WATERLINE_TEXT_CMD", False);

        Atom type_atom = XInternAtom(dpy, "UTF8_STRING", False);

        XChangeProperty (dpy, root, cmd_atom, type_atom, 8, PropModeReplace,
                         (unsigned char *) buff, buff_size);
        XSync(dpy, False);
        XCloseDisplay(dpy);
    }

    return 0;
}

