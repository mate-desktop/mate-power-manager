/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8 -*-
 *
 * On-screen-display (OSD) window for mate-settings-daemon's plugins
 *
 * Copyright (C) 2006 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2009 Novell, Inc
 *
 * Authors:
 *   William Jon McCann <mccann@jhu.edu>
 *   Federico Mena-Quintero <federico@novell.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

/* MsdOsdWindow is an "on-screen-display" window (OSD).  It is the cute,
 * semi-transparent, curved popup that appears when you press a hotkey global to
 * the desktop, such as to change the volume, switch your monitor's parameters,
 * etc.
 *
 * You can create a MsdOsdWindow and use it as a normal GtkWindow.  It will
 * automatically center itself, figure out if it needs to be composited, etc.
 * Just pack your widgets in it, sit back, and enjoy the ride.
 */

#ifndef MSD_OSD_WINDOW_H
#define MSD_OSD_WINDOW_H

#include <glib-object.h>
#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Alpha value to be used for foreground objects drawn in an OSD window */
#define MSD_OSD_WINDOW_FG_ALPHA 1.0

#define MSD_TYPE_OSD_WINDOW            (msd_osd_window_get_type ())
#define MSD_OSD_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj),  MSD_TYPE_OSD_WINDOW, MsdOsdWindow))
#define MSD_OSD_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),   MSD_TYPE_OSD_WINDOW, MsdOsdWindowClass))
#define MSD_IS_OSD_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj),  MSD_TYPE_OSD_WINDOW))
#define MSD_IS_OSD_WINDOW_CLASS(klass) (G_TYPE_INSTANCE_GET_CLASS ((klass), MSD_TYPE_OSD_WINDOW))
#define MSD_OSD_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MSD_TYPE_OSD_WINDOW, MsdOsdWindowClass))

typedef struct MsdOsdWindow                   MsdOsdWindow;
typedef struct MsdOsdWindowClass              MsdOsdWindowClass;
typedef struct MsdOsdWindowPrivate            MsdOsdWindowPrivate;

struct MsdOsdWindow {
        GtkWindow                   parent;

        MsdOsdWindowPrivate  *priv;
};

struct MsdOsdWindowClass {
        GtkWindowClass parent_class;

        void (* draw_when_composited) (MsdOsdWindow *window, cairo_t *cr);
};

GType                 msd_osd_window_get_type          (void);

GtkWidget *           msd_osd_window_new               (void);
gboolean              msd_osd_window_is_composited     (MsdOsdWindow      *window);
gboolean              msd_osd_window_is_valid          (MsdOsdWindow      *window);
void                  msd_osd_window_update_and_hide   (MsdOsdWindow      *window);

#ifdef __cplusplus
}
#endif

#endif
