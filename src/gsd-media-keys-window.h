/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8 -*-
 *
 * Copyright (C) 2006 William Jon McCann <mccann@jhu.edu>
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

#ifndef MSD_MEDIA_KEYS_WINDOW_H
#define MSD_MEDIA_KEYS_WINDOW_H

#include <glib-object.h>
#include <gtk/gtk.h>

#include "msd-osd-window.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MSD_TYPE_MEDIA_KEYS_WINDOW            (msd_media_keys_window_get_type ())
#define MSD_MEDIA_KEYS_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj),  MSD_TYPE_MEDIA_KEYS_WINDOW, MsdMediaKeysWindow))
#define MSD_MEDIA_KEYS_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),   MSD_TYPE_MEDIA_KEYS_WINDOW, MsdMediaKeysWindowClass))
#define MSD_IS_MEDIA_KEYS_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj),  MSD_TYPE_MEDIA_KEYS_WINDOW))
#define MSD_IS_MEDIA_KEYS_WINDOW_CLASS(klass) (G_TYPE_INSTANCE_GET_CLASS ((klass), MSD_TYPE_MEDIA_KEYS_WINDOW))

typedef struct MsdMediaKeysWindow                   MsdMediaKeysWindow;
typedef struct MsdMediaKeysWindowClass              MsdMediaKeysWindowClass;
typedef struct MsdMediaKeysWindowPrivate            MsdMediaKeysWindowPrivate;

struct MsdMediaKeysWindow {
        MsdOsdWindow parent;

        MsdMediaKeysWindowPrivate  *priv;
};

struct MsdMediaKeysWindowClass {
        MsdOsdWindowClass parent_class;
};

typedef enum {
        MSD_MEDIA_KEYS_WINDOW_ACTION_VOLUME,
        MSD_MEDIA_KEYS_WINDOW_ACTION_CUSTOM
} MsdMediaKeysWindowAction;

GType                 msd_media_keys_window_get_type          (void);

GtkWidget *           msd_media_keys_window_new               (void);
void                  msd_media_keys_window_set_action        (MsdMediaKeysWindow      *window,
                                                               MsdMediaKeysWindowAction action);
void                  msd_media_keys_window_set_action_custom (MsdMediaKeysWindow      *window,
                                                               const char              *icon_name,
                                                               gboolean                 show_level);
void                  msd_media_keys_window_set_volume_muted  (MsdMediaKeysWindow      *window,
                                                               gboolean                 muted);
void                  msd_media_keys_window_set_volume_level  (MsdMediaKeysWindow      *window,
                                                               int                      level);
gboolean              msd_media_keys_window_is_valid          (MsdMediaKeysWindow      *window);

#ifdef __cplusplus
}
#endif

#endif
