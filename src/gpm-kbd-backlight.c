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

#include <gio/gio.h>
#include <glib.h>
#include <libupower-glib/upower.h>
#include <gtk/gtk.h>

#include "egg-debug.h"
#include "gpm-button.h"
#include "gpm-common.h"
#include "gpm-control.h"
#include "gpm-idle.h"
#include "gpm-kbd-backlight.h"
#include "gsd-media-keys-window.h"

#define GPM_KBD_BACKLIGHT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_KBD_BACKLIGHT, GpmKbdBacklightPrivate))

struct GpmKbdBacklightPrivate
{
    UpClient        *client;
    GpmButton       *button;
    GSettings       *settings;
    GpmControl      *control;
    GpmIdle         *idle;
    gboolean         can_dim;
    gboolean         system_is_idle;
    GTimer          *idle_timer;
    guint            idle_dim_timeout;
    guint            master_percentage;
    guint            brightness;
    guint            max_brightness;
    guint            brightness_percent;
    GDBusProxy      *upower_proxy;
    GDBusConnection     *bus_connection;
    guint            bus_object_id;
    GtkWidget		*popup;
};

enum {
   BRIGHTNESS_CHANGED,
   LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GpmKbdBacklight, gpm_kbd_backlight, G_TYPE_OBJECT)

/**
 * gpm_kbd_backlight_error_quark:
 * Return value: Our personal error quark.
 **/
GQuark
gpm_kbd_backlight_error_quark (void)
{
   static GQuark quark = 0;
   if (!quark)
       quark = g_quark_from_static_string ("gpm_kbd_backlight_error");
   return quark;
}

/**
 * gpm_kbd_backlight_get_brightness:
 * @backlight:
 * @brightness:
 * @error:
 *
 * Return value:
 */
gboolean
gpm_kbd_backlight_get_brightness (GpmKbdBacklight *backlight,
                 guint *brightness,
                 GError **error)
{
   g_return_val_if_fail (backlight != NULL, FALSE);
   g_return_val_if_fail (GPM_IS_KBD_BACKLIGHT (backlight), FALSE);
   g_return_val_if_fail (brightness != NULL, FALSE);

   if (backlight->priv->can_dim == FALSE) {
       g_set_error_literal (error, gpm_kbd_backlight_error_quark (),
                    GPM_KBD_BACKLIGHT_ERROR_HARDWARE_NOT_PRESENT,
                    "Dim capable hardware not present");
       return FALSE;
   }

   *brightness = backlight->priv->brightness_percent;
   return TRUE;
}

static gboolean
gpm_kbd_backlight_set (GpmKbdBacklight *backlight,
              guint percentage)
{
   gint scale;
   guint goal;

   g_return_val_if_fail (GPM_IS_KBD_BACKLIGHT (backlight), FALSE);
   /* avoid warnings if no keyboard brightness is available */
   if (backlight->priv->max_brightness < 1)
       return FALSE;
   /* if we're setting the same we are, don't bother */
   //g_return_val_if_fail (backlight->priv->brightness_percent != percentage, FALSE);

   goal = gpm_discrete_from_percent (percentage, backlight->priv->max_brightness);
   scale = percentage > backlight->priv->brightness_percent ? 1 : -1;

   /* if percentage change too small force next value */
   if (goal == backlight->priv->brightness) {
       goal += percentage == backlight->priv->brightness_percent ? 0 : scale;
   }

   /* step loop down by 1 for a dimming effect */
   while (backlight->priv->brightness != goal) {
       backlight->priv->brightness += scale;
       backlight->priv->brightness_percent = gpm_discrete_to_percent (backlight->priv->brightness, backlight->priv->max_brightness);

       g_dbus_proxy_call (backlight->priv->upower_proxy,
                      "SetBrightness",
                   g_variant_new ("(i)", (gint) backlight->priv->brightness),
                   G_DBUS_CALL_FLAGS_NONE,
                   -1,
                   NULL,
                   NULL,
                   NULL);
   }
    egg_debug("Set brightness to %i", backlight->priv->brightness);
   return TRUE;
}

/** 
 * gpm_kbd_backlight_dialog_init
 **/
static void
gpm_kbd_backlight_dialog_init (GpmKbdBacklight *backlight) 
{  
    if (backlight->priv->popup != NULL
	    && !msd_osd_window_is_valid (MSD_OSD_WINDOW (backlight->priv->popup))) {
		gtk_widget_destroy (backlight->priv->popup);
		backlight->priv->popup = NULL;
	}

	if (backlight->priv->popup == NULL) {
		backlight->priv->popup= msd_media_keys_window_new ();
		msd_media_keys_window_set_action_custom (MSD_MEDIA_KEYS_WINDOW (backlight->priv->popup),
							 "gpm-brightness-kbd",
							 TRUE);
		gtk_window_set_position (GTK_WINDOW (backlight->priv->popup), GTK_WIN_POS_NONE);

    }
}
/**
 * gpm_kbd_backlight_dialog_show:
 *
 * Show the brightness popup, and place it nicely on the screen.
 **/
static void
gpm_kbd_backlight_dialog_show (GpmKbdBacklight *backlight)
{
	int            orig_w;
	int            orig_h;
	int            screen_w;
	int            screen_h;
	int            x;
	int            y;
	int            pointer_x;
	int            pointer_y;
	GtkRequisition win_req;
	GdkScreen     *pointer_screen;
	GdkRectangle   geometry;
#if GTK_CHECK_VERSION (3, 22, 0)
	GdkMonitor    *monitor;
#else
	int            monitor;
#endif
        GdkDisplay    *display;
        GdkDeviceManager *device_manager;
        GdkDevice     *device;

	/*
	 * get the window size
	 * if the window hasn't been mapped, it doesn't necessarily
	 * know its true size, yet, so we need to jump through hoops
	 */
	gtk_window_get_default_size (GTK_WINDOW (backlight->priv->popup), &orig_w, &orig_h);
	gtk_widget_get_preferred_size (backlight->priv->popup, NULL, &win_req);

	if (win_req.width > orig_w) {
		orig_w = win_req.width;
	}
	if (win_req.height > orig_h) {
		orig_h = win_req.height;
	}

	pointer_screen = NULL;
        display = gtk_widget_get_display (backlight->priv->popup);
        device_manager = gdk_display_get_device_manager (display);
        device = gdk_device_manager_get_client_pointer (device_manager);
        gdk_device_get_position (device,
				 &pointer_screen,
				 &pointer_x,
				 &pointer_y);

#if GTK_CHECK_VERSION (3, 22, 0)
	monitor = gdk_display_get_monitor_at_point (gdk_screen_get_display (pointer_screen),
						    pointer_x,
						    pointer_y);

	gdk_monitor_get_geometry (monitor, &geometry);
#else
	monitor = gdk_screen_get_monitor_at_point (pointer_screen,
						   pointer_x,
						   pointer_y);

	gdk_screen_get_monitor_geometry (pointer_screen,
					 monitor,
					 &geometry);
#endif

	screen_w = geometry.width;
	screen_h = geometry.height;

	x = ((screen_w - orig_w) / 2) + geometry.x;
	y = geometry.y + (screen_h / 2) + (screen_h / 2 - orig_h) / 2;

	gtk_window_move (GTK_WINDOW (backlight->priv->popup), x, y);

	gtk_widget_show (backlight->priv->popup);

	gdk_display_sync (gtk_widget_get_display (backlight->priv->popup));
}

/**
 * gpm_kbd_backlight_brightness_up:
 **/
static gboolean
gpm_kbd_backlight_brightness_up (GpmKbdBacklight *backlight)
{
   guint new;

   new = MIN (backlight->priv->brightness_percent + GPM_KBD_BACKLIGHT_STEP, 100u);
   return gpm_kbd_backlight_set (backlight, new);
}

/**
 * gpm_kbd_backlight_brightness_down:
 **/
static gboolean
gpm_kbd_backlight_brightness_down (GpmKbdBacklight *backlight)
{
   guint new;

   // we can possibly go below 0 here, so by converting to a gint we avoid underflow errors.
   new = MAX ((gint) backlight->priv->brightness_percent - GPM_KBD_BACKLIGHT_STEP, 0);
   return gpm_kbd_backlight_set (backlight, new);
}

/**
 * gpm_kbd_backlight_set_brightness:
 * @backlight:
 * @percentage:
 * @error:
 *
 * Return value:
 **/
gboolean
gpm_kbd_backlight_set_brightness (GpmKbdBacklight *backlight,
                 guint percentage,
                 GError **error)
{
   gboolean ret;

   g_return_val_if_fail (backlight != NULL, FALSE);
   g_return_val_if_fail (GPM_IS_KBD_BACKLIGHT (backlight), FALSE);

   if (backlight->priv->can_dim == FALSE) {
       g_set_error_literal (error, gpm_kbd_backlight_error_quark (),
                    GPM_KBD_BACKLIGHT_ERROR_HARDWARE_NOT_PRESENT,
                    "Dim capable hardware not present");
       return FALSE;
   }

   backlight->priv->master_percentage = percentage;

   ret = gpm_kbd_backlight_set (backlight, percentage);
   if (!ret) {
       g_set_error_literal (error, gpm_kbd_backlight_error_quark (),
                    GPM_KBD_BACKLIGHT_ERROR_GENERAL,
                    "Cannot set keyboard backlight brightness");
   }

   return ret;
}

static void
gpm_kbd_backlight_on_brightness_changed (GpmKbdBacklight *backlight,
                    guint value)
{
   backlight->priv->brightness = value;
   backlight->priv->brightness_percent = gpm_discrete_to_percent (value, backlight->priv->max_brightness);
   backlight->priv->master_percentage = backlight->priv->brightness_percent;
   g_signal_emit (backlight, signals [BRIGHTNESS_CHANGED], 0, backlight->priv->brightness_percent);
}

/**
 * gpm_kbd_backlight_on_dbus_signal:
 **/
static void
gpm_kbd_backlight_on_dbus_signal (GDBusProxy *proxy,
                 gchar      *sender_name,
                 gchar      *signal_name,
                 GVariant   *parameters,
                 gpointer    user_data)
{
   guint value;
   GpmKbdBacklight *backlight = GPM_KBD_BACKLIGHT (user_data);

   if (g_strcmp0 (signal_name, "BrightnessChanged") == 0) {
       g_variant_get (parameters, "(i)", &value);
       gpm_kbd_backlight_on_brightness_changed (backlight, value);
       return;
   }

   g_debug ("signal '%s' not handled!", signal_name);
}

/**
 * gpm_kbd_backlight_dbus_method_call:
 * @connection:
 * @object_path:
 * @interface_name:
 * @method_name:
 * @parameters:
 * @invocation:
 * @user_data:
 **/
static void
gpm_kbd_backlight_dbus_method_call (GDBusConnection *connection,
                   const gchar *sender,
                   const gchar *object_path,
                   const gchar *interface_name,
                   const gchar *method_name,
                   GVariant *parameters,
                   GDBusMethodInvocation *invocation,
                   gpointer user_data)
{
   guint value;
   gboolean ret;
   GError *error = NULL;
   GpmKbdBacklight *backlight = GPM_KBD_BACKLIGHT (user_data);

   if (g_strcmp0 (method_name, "GetBrightness") == 0) {
       ret = gpm_kbd_backlight_get_brightness (backlight, &value, &error);
       if (!ret) {
           g_dbus_method_invocation_return_gerror (invocation, error);
           g_error_free (error);
       } else {
           g_dbus_method_invocation_return_value (invocation, g_variant_new ("(u)", value));
       }
       return;
   }

   if (g_strcmp0 (method_name, "SetBrightness") == 0) {
       g_variant_get (parameters, "(u)", &value);
       ret = gpm_kbd_backlight_set_brightness (backlight, value, &error);
       if (!ret) {
           g_dbus_method_invocation_return_gerror (invocation, error);
           g_error_free (error);
       } else {
           g_dbus_method_invocation_return_value (invocation, NULL);
       }
       return;
   }

   g_assert_not_reached ();
}


/**
 * gpm_kbd_backlight_dbus_property_get:
 * @sender:
 * @object_path:
 * @interface_name:
 * @property_name:
 * @error:
 * @user_data:
 *
 * Return value:
 **/
static GVariant *
gpm_kbd_backlight_dbus_property_get (GDBusConnection *connection,
                    const gchar *sender,
                    const gchar *object_path,
                    const gchar *interface_name,
                    const gchar *property_name,
                    GError **error,
                    gpointer user_data)
{
   /* Do nothing, we have no props */
   return NULL;
}

/**
 * gpm_kbd_backlight_dbus_property_set:
 * @connection:
 * @sender:
 * @object_path:
 * @interface_name:
 * @property_name:
 *
 * Return value:
 **/
static gboolean
gpm_kbd_backlight_dbus_property_set (GDBusConnection *connection,
                    const gchar *sender,
                    const gchar *object_path,
                        const gchar *interface_name,
                        const gchar *property_name,
                    GVariant *value,
                    GError **error,
                    gpointer user_data)
{
   /* do nothing, no properties defined */
   return FALSE;
}

static gboolean
gpm_kbd_backlight_evaluate_power_source_and_set (GpmKbdBacklight *backlight)
{
   gfloat brightness;
   gfloat scale;
   gboolean on_battery;
   gboolean battery_reduce;
   guint value;
   gboolean ret;

   brightness = backlight->priv->master_percentage;

   g_object_get (backlight->priv->client,
             "on-battery",
             &on_battery,
             NULL);

   battery_reduce = g_settings_get_boolean (backlight->priv->settings, GPM_SETTINGS_KBD_BACKLIGHT_BATT_REDUCE);

   if (on_battery && battery_reduce) {
       value = g_settings_get_int (backlight->priv->settings, GPM_SETTINGS_KBD_BRIGHTNESS_DIM_BY_ON_BATT);

       if (value > 100) {
           g_warning ("Cannot scale brightness down by more than 100%%. Scaling by 50%%");
           value = 50;
       }

       scale = (100 - value) / 100.0f;
       brightness *= scale;

       value = (guint) brightness;

   } else {
       value = g_settings_get_int (backlight->priv->settings, GPM_SETTINGS_KBD_BRIGHTNESS_ON_AC);
   }

   ret = gpm_kbd_backlight_set (backlight, value);
   return ret;
}

/**
 * gpm_kbd_backlight_control_resume_cb:
 * @control: The control class instance
 * @backlight: This backlight class instance
 *
 * Just make sure that the backlight is back on
 **/
static void
gpm_kbd_backlight_control_resume_cb (GpmControl *control,
                    GpmControlAction action,
                    GpmKbdBacklight *backlight)
{
   gboolean ret;

   ret = gpm_kbd_backlight_evaluate_power_source_and_set (backlight);
   if (!ret)
       g_warning ("Failed to turn kbd brightness back on after resuming");
}

/**
 * gpm_kbd_backlight_client_changed_cb:
 * @client: The up_client class instance
 * @backlight: This class instance
 *
 * Does the actions when the ac power source is inserted/removed.
 **/
static void
gpm_kbd_backlight_client_changed_cb (UpClient *client,
#if UP_CHECK_VERSION(0, 99, 0)
                    GParamSpec *pspec,
#endif
                    GpmKbdBacklight *backlight)
{
   gpm_kbd_backlight_evaluate_power_source_and_set (backlight);
}

/**
 * gpm_kbd_backlight_button_pressed_cb:
 * @power: The power class instance
 * @type: The button type, but here we only care about keyboard brightness buttons
 * @backlight: This class instance
 **/
static void
gpm_kbd_backlight_button_pressed_cb (GpmButton *button,
                    const gchar *type,
                    GpmKbdBacklight *backlight)
{
   static guint saved_brightness;
   gboolean ret;

   saved_brightness = backlight->priv->master_percentage;

   if (g_strcmp0 (type, GPM_BUTTON_KBD_BRIGHT_UP) == 0) {
       ret = gpm_kbd_backlight_brightness_up (backlight);
        
        if (ret) {
            egg_debug("Going to display OSD");
            gpm_kbd_backlight_dialog_init (backlight);
			msd_media_keys_window_set_volume_level (MSD_MEDIA_KEYS_WINDOW (backlight->priv->popup), backlight->priv->brightness_percent);
            gpm_kbd_backlight_dialog_show (backlight);
        }

   } else if (g_strcmp0 (type, GPM_BUTTON_KBD_BRIGHT_DOWN) == 0) {
       ret = gpm_kbd_backlight_brightness_down (backlight);

        if (ret) {
            egg_debug("Going to display OSD");
            gpm_kbd_backlight_dialog_init (backlight);
			msd_media_keys_window_set_volume_level (MSD_MEDIA_KEYS_WINDOW (backlight->priv->popup), backlight->priv->brightness_percent);
            gpm_kbd_backlight_dialog_show (backlight);
        }
        
   } else if (g_strcmp0 (type, GPM_BUTTON_KBD_BRIGHT_TOGGLE) == 0) {
       if (backlight->priv->master_percentage == 0) {
           /* backlight is off turn it back on */
           gpm_kbd_backlight_set (backlight, saved_brightness);
       } else {
           /* backlight is on, turn it off and save current value */
           saved_brightness = backlight->priv->master_percentage;
           gpm_kbd_backlight_set (backlight, 0);
       }
   }
}

/**
 * gpm_kbd_backlight_idle_changed_cb:
 * @idle: The idle class instance
 * @mode: The idle mode, e.g. GPM_IDLE_MODE_BLANK
 * @backlight: This class instance
 *
 * This callback is called when mate-screensaver detects that the idle state
 * has changed. GPM_IDLE_MODE_BLANK is when the session has become inactive,
 * and GPM_IDLE_MODE_SLEEP is where the session has become inactive, AND the
 * session timeout has elapsed for the idle action.
 **/
static void
gpm_kbd_backlight_idle_changed_cb (GpmIdle *idle,
                  GpmIdleMode mode,
                  GpmKbdBacklight *backlight)
{
   gfloat brightness;
   gfloat scale;
   guint value;
   gboolean lid_closed;
   gboolean on_battery;
   gboolean enable_action;

    egg_debug("Idle changed");

   lid_closed = gpm_button_is_lid_closed (backlight->priv->button);

   if (lid_closed)
       return;

   g_object_get (backlight->priv->client,
                 "on-battery",
                 &on_battery,
                 NULL);

   enable_action = on_battery
       ? g_settings_get_boolean (backlight->priv->settings, GPM_SETTINGS_IDLE_DIM_BATT)
       : g_settings_get_boolean (backlight->priv->settings, GPM_SETTINGS_IDLE_DIM_AC);

   if (!enable_action)
       return;

   if (mode == GPM_IDLE_MODE_NORMAL) {
        egg_debug("GPM_IDLE_MODE_NORMAL");
       backlight->priv->master_percentage = 100;
       gpm_kbd_backlight_evaluate_power_source_and_set (backlight);
   } else if (mode == GPM_IDLE_MODE_DIM) {
       egg_debug("GPM_IDLE_MODE_DIM");
       brightness = backlight->priv->master_percentage;
       value = g_settings_get_int (backlight->priv->settings, GPM_SETTINGS_KBD_BRIGHTNESS_DIM_BY_ON_IDLE);

       if (value > 100) {
           egg_warning ("Cannot scale brightness down by more than 100%%. Scaling by 50%%");
           value = 50;
       }

       scale = (100 - value) / 100.0f;
       brightness *= scale;

       value = (guint) brightness;
       gpm_kbd_backlight_set (backlight, value);
   } else if (mode == GPM_IDLE_MODE_BLANK) {
       gpm_kbd_backlight_set (backlight, 0u);
   }
}

/**
 * gpm_kbd_backlight_finalize:
 * @object:
 **/
static void
gpm_kbd_backlight_finalize (GObject *object)
{
   GpmKbdBacklight *backlight;

   g_return_if_fail (object != NULL);
   g_return_if_fail (GPM_IS_KBD_BACKLIGHT (object));

   backlight = GPM_KBD_BACKLIGHT (object);

   if (backlight->priv->upower_proxy != NULL) {
       g_object_unref (backlight->priv->upower_proxy);
   }
   if (backlight->priv->bus_connection != NULL) {
       g_dbus_connection_unregister_object (backlight->priv->bus_connection,
                            backlight->priv->bus_object_id);
       g_object_unref (backlight->priv->bus_connection);
   }

   g_timer_destroy (backlight->priv->idle_timer);

   g_object_unref (backlight->priv->control);
   g_object_unref (backlight->priv->settings);
   g_object_unref (backlight->priv->client);
   g_object_unref (backlight->priv->button);
   g_object_unref (backlight->priv->idle);

   g_return_if_fail (backlight->priv != NULL);
   G_OBJECT_CLASS (gpm_kbd_backlight_parent_class)->finalize (object);
}

/**
 * gpm_kbd_backlight_class_init:
 * @klass:
 **/
static void
gpm_kbd_backlight_class_init (GpmKbdBacklightClass *klass)
{
   GObjectClass *object_class = G_OBJECT_CLASS (klass);
   object_class->finalize = gpm_kbd_backlight_finalize;

   signals [BRIGHTNESS_CHANGED] =
       g_signal_new ("brightness-changed",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GpmKbdBacklightClass, brightness_changed),
                  NULL,
                  NULL,
                  g_cclosure_marshal_VOID__UINT,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_UINT);

   g_type_class_add_private (klass, sizeof (GpmKbdBacklightPrivate));
}

/**
 * gpm_kbd_backlight_init:
 * @backlight: This KbdBacklight class instance
 *
 * Initializes the KbdBacklight class.
 **/
static void
gpm_kbd_backlight_init (GpmKbdBacklight *backlight)
{
   GVariant *u_brightness;
   GVariant *u_max_brightness;
   GError   *error = NULL;

   backlight->priv = GPM_KBD_BACKLIGHT_GET_PRIVATE (backlight);

   backlight->priv->upower_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                      G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                      NULL,
                                      "org.freedesktop.UPower",
                                      "/org/freedesktop/UPower/KbdBacklight",
                                      "org.freedesktop.UPower.KbdBacklight",
                                      NULL,
                                      &error);
   if (backlight->priv->upower_proxy == NULL) {
       g_printerr ("Could not connect to UPower system bus: %s", error->message);
       g_error_free (error);
       goto err;
   }

   g_signal_connect (backlight->priv->upower_proxy,
             "g-signal",
             G_CALLBACK (gpm_kbd_backlight_on_dbus_signal),
             backlight);

   u_brightness = g_dbus_proxy_call_sync (backlight->priv->upower_proxy,
                          "GetBrightness",
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          &error);
   if (u_brightness == NULL) {
       if (error->domain != G_DBUS_ERROR || error->code != G_DBUS_ERROR_UNKNOWN_METHOD)
           g_warning ("Failed to get brightness: %s", error->message);
       g_error_free (error);
       goto err;
   }

   error = NULL;
   u_max_brightness = g_dbus_proxy_call_sync (backlight->priv->upower_proxy,
                          "GetMaxBrightness",
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          &error);
   if (u_max_brightness == NULL) {
       g_warning ("Failed to get max brightness: %s", error->message);
       g_error_free (error);
       g_variant_unref (u_brightness);
       goto err;
   }

   g_variant_get (u_brightness, "(i)", &backlight->priv->brightness);
   g_variant_get (u_max_brightness, "(i)", &backlight->priv->max_brightness);

   backlight->priv->brightness_percent = gpm_discrete_to_percent (backlight->priv->brightness,
                                      backlight->priv->max_brightness);

   g_variant_unref (u_brightness);
   g_variant_unref (u_max_brightness);
   goto noerr;

err:
   backlight->priv->brightness = 0;
   backlight->priv->brightness_percent = 100;
   backlight->priv->max_brightness = 0;

noerr:
   /* Initialize the master to full power. It will get scaled if needed */
   backlight->priv->master_percentage = 100u;

   backlight->priv->idle_timer = g_timer_new ();
   backlight->priv->can_dim = backlight->priv->max_brightness > 1;

   /* Use upower for ac changed signal */
   backlight->priv->client = up_client_new ();
#if UP_CHECK_VERSION(0, 99, 0)
   g_signal_connect (backlight->priv->client, "notify",
             G_CALLBACK (gpm_kbd_backlight_client_changed_cb), backlight);
#else
   g_signal_connect (backlight->priv->client, "changed",
             G_CALLBACK (gpm_kbd_backlight_client_changed_cb), backlight);
#endif

   backlight->priv->settings = g_settings_new (GPM_SETTINGS_SCHEMA);

   /* watch for kbd brightness up and down button presses */
   backlight->priv->button = gpm_button_new ();
   g_signal_connect (backlight->priv->button, "button-pressed",
             G_CALLBACK (gpm_kbd_backlight_button_pressed_cb), backlight);

   backlight->priv->idle = gpm_idle_new ();
   g_signal_connect (backlight->priv->idle, "idle-changed",
             G_CALLBACK (gpm_kbd_backlight_idle_changed_cb), backlight);

    /* use a visual widget */
   backlight->priv->popup = msd_media_keys_window_new ();
   msd_media_keys_window_set_action_custom (MSD_MEDIA_KEYS_WINDOW (backlight->priv->popup),
                                            "gpm-brightness-kbd", TRUE);
   gtk_window_set_position (GTK_WINDOW (backlight->priv->popup), GTK_WIN_POS_NONE);

   /* since gpm is just starting we can pretty safely assume that we're not idle */
   backlight->priv->system_is_idle = FALSE;
   backlight->priv->idle_dim_timeout = g_settings_get_int (backlight->priv->settings, GPM_SETTINGS_IDLE_DIM_TIME);
   gpm_idle_set_timeout_dim (backlight->priv->idle, backlight->priv->idle_dim_timeout);

   /* make sure we turn the keyboard backlight back on after resuming */
   backlight->priv->control = gpm_control_new ();
   g_signal_connect (backlight->priv->control, "resume",
             G_CALLBACK (gpm_kbd_backlight_control_resume_cb), backlight);

   /* set initial values for whether we're on AC or battery*/
   gpm_kbd_backlight_evaluate_power_source_and_set (backlight);
}

/**
 * gpm_kbd_backlight_new:
 * Return value: A new GpmKbdBacklight class instance.
 **/
GpmKbdBacklight *
gpm_kbd_backlight_new (void)
{
   GpmKbdBacklight *backlight = g_object_new (GPM_TYPE_KBD_BACKLIGHT, NULL);
   return backlight;
}

