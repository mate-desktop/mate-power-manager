/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Alex Launi <alex launi canonical com>
 *
 * Licensed under the GNU General Public License Version 2
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __GPM_KBD_BACKLIGHT_H
#define __GPM_KBD_BACKLIGHT_H

#include <gio/gio.h>
#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_TYPE_KBD_BACKLIGHT     (gpm_kbd_backlight_get_type ())
#define GPM_KBD_BACKLIGHT(o)       (G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_KBD_BACKLIGHT, GpmKbdBacklight))
#define GPM_KBD_BACKLIGHT_CLASS(k) (G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_KBD_BACKLIGHT, GpmKbdBacklightClass))
#define GPM_IS_KBD_BACKLIGHT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_KBD_BACKLIGHT))
#define GPM_IS_KBD_BACKLIGHT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_KBD_BACKLIGHT))
#define GPM_KBD_BACKLIGHT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_KBD_BACKLIGHT, GpmKbdBacklightClass))

#define GPM_KBD_BACKLIGHT_DIM_INTERVAL 5 /* ms */
#define GPM_KBD_BACKLIGHT_STEP 10 /* change by 10% each step */

typedef struct GpmKbdBacklightPrivate GpmKbdBacklightPrivate;

typedef struct
{
   GObject         parent;
   GpmKbdBacklightPrivate *priv;
} GpmKbdBacklight;

typedef struct
{
   GObjectClass parent_class;
   void         (* brightness_changed) (GpmKbdBacklight *backlight,
                        gint         brightness);
} GpmKbdBacklightClass;

typedef enum
{
    GPM_KBD_BACKLIGHT_ERROR_GENERAL,
    GPM_KBD_BACKLIGHT_ERROR_DATA_NOT_AVAILABLE,
    GPM_KBD_BACKLIGHT_ERROR_HARDWARE_NOT_PRESENT
} GpmKbdBacklightError;

GType          gpm_kbd_backlight_get_type      (void);
GQuark         gpm_kbd_backlight_error_quark       (void);
GpmKbdBacklight           *gpm_kbd_backlight_new           (void);
gboolean       gpm_kbd_backlight_get_brightness    (GpmKbdBacklight *backlight,
                                guint *brightness,
                                GError **error);
gboolean       gpm_kbd_backlight_set_brightness    (GpmKbdBacklight *backlight,
                                guint brightness,
                                GError **error);
void           gpm_kbd_backlight_register_dbus     (GpmKbdBacklight *backlight,
                                GDBusConnection *connection,
                                GError **error);

G_END_DECLS

#endif /* __GPM_KBD_BACKLIGHT_H */

