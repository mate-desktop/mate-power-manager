/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2005-2009 Richard Hughes <richard@hughsie.com>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libupower-glib/upower.h>

#include "egg-debug.h"

#include "gpm-upower.h"
#include "gpm-engine.h"
#include "gpm-common.h"
#include "gpm-icon-names.h"
#include "gpm-tray-icon.h"

static void     gpm_tray_icon_finalize   (GObject	   *object);

#define GPM_TRAY_ICON_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_TYPE_TRAY_ICON, GpmTrayIconPrivate))

struct GpmTrayIconPrivate
{
	GSettings		*settings;
	GpmEngine		*engine;
	GtkStatusIcon		*status_icon;
	gboolean		 show_actions;
};

G_DEFINE_TYPE (GpmTrayIcon, gpm_tray_icon, G_TYPE_OBJECT)

/**
 * gpm_tray_icon_enable_actions:
 **/
static void
gpm_tray_icon_enable_actions (GpmTrayIcon *icon, gboolean enabled)
{
	g_return_if_fail (GPM_IS_TRAY_ICON (icon));
	icon->priv->show_actions = enabled;
}

/**
 * gpm_tray_icon_show:
 * @enabled: If we should show the tray
 **/
static void
gpm_tray_icon_show (GpmTrayIcon *icon, gboolean enabled)
{
	g_return_if_fail (GPM_IS_TRAY_ICON (icon));
	gtk_status_icon_set_visible (icon->priv->status_icon, enabled);
}

/**
 * gpm_tray_icon_set_tooltip:
 * @tooltip: The tooltip text, e.g. "Batteries charged"
 **/
gboolean
gpm_tray_icon_set_tooltip (GpmTrayIcon *icon, const gchar *tooltip)
{
	g_return_val_if_fail (icon != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_TRAY_ICON (icon), FALSE);
	g_return_val_if_fail (tooltip != NULL, FALSE);

	gtk_status_icon_set_tooltip_text (icon->priv->status_icon, tooltip);

	return TRUE;
}

/**
 * gpm_tray_icon_get_status_icon:
 **/
GtkStatusIcon *
gpm_tray_icon_get_status_icon (GpmTrayIcon *icon)
{
	g_return_val_if_fail (GPM_IS_TRAY_ICON (icon), NULL);
	return g_object_ref (icon->priv->status_icon);
}

/**
 * gpm_tray_icon_set_icon:
 * @icon_name: The icon name, e.g. GPM_ICON_APP_ICON, or NULL to remove.
 *
 * Loads a pixmap from disk, and sets as the tooltip icon.
 **/
gboolean
gpm_tray_icon_set_icon (GpmTrayIcon *icon, const gchar *icon_name)
{
	g_return_val_if_fail (icon != NULL, FALSE);
	g_return_val_if_fail (GPM_IS_TRAY_ICON (icon), FALSE);

	if (icon_name != NULL) {
		egg_debug ("Setting icon to %s", icon_name);
		gtk_status_icon_set_from_icon_name (icon->priv->status_icon,
		                                    icon_name);

		/* make sure that we are visible */
		gpm_tray_icon_show (icon, TRUE);
	} else {
		/* remove icon */
		egg_debug ("no icon will be displayed");

		/* make sure that we are hidden */
		gpm_tray_icon_show (icon, FALSE);
	}
	return TRUE;
}

/**
 * gpm_tray_icon_show_info_cb:
 **/
static void
gpm_tray_icon_show_info_cb (GtkMenuItem *item, gpointer data)
{
	gchar *path;
	const gchar *object_path;

	object_path = g_object_get_data (G_OBJECT (item), "object-path");
	path = g_strdup_printf ("%s/mate-power-statistics --device %s", BINDIR, object_path);
	if (!g_spawn_command_line_async (path, NULL))
		egg_warning ("Couldn't execute command: %s", path);
	g_free (path);
}

/**
 * gpm_tray_icon_show_preferences_cb:
 * @action: A valid GtkAction
 **/
static void
gpm_tray_icon_show_preferences_cb (GtkMenuItem *item, gpointer data)
{
	const gchar *command = "mate-power-preferences";

	if (g_spawn_command_line_async (command, NULL) == FALSE)
		egg_warning ("Couldn't execute command: %s", command);
}

/**
 * gpm_tray_icon_show_about_cb:
 * @action: A valid GtkAction
 **/
static void
gpm_tray_icon_show_about_cb (GtkMenuItem *item, gpointer data)
{
	const gchar *authors[] =
	{
		"Perberos",
		"Steve Zesch",
		"Stefano Karapetsas",
		NULL
	};

	gtk_show_about_dialog (NULL,
				"program-name", _("Power Manager"),
				"version", VERSION,
				"comments", _("Power management daemon"),
				"copyright", _("Copyright \xC2\xA9 2011-2017 MATE developers"),
				"authors", authors,
				/* Translators should localize the following string
				* which will be displayed at the bottom of the about
				* box to give credit to the translator(s).
				*/
				"translator-credits", _("translator-credits"),
				"logo-icon-name", "mate-power-manager",
				"website", "http://www.mate-desktop.org",
				NULL);
}

/**
 * gpm_tray_icon_class_init:
 **/
static void
gpm_tray_icon_class_init (GpmTrayIconClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gpm_tray_icon_finalize;
	g_type_class_add_private (klass, sizeof (GpmTrayIconPrivate));
}

/**
 * gpm_tray_icon_add_device:
 **/
static guint
gpm_tray_icon_add_device (GpmTrayIcon *icon, GtkMenu *menu, const GPtrArray *array, UpDeviceKind kind)
{
	guint i;
	guint added = 0;
	gchar *icon_name;
	gchar *label, *vendor, *model;
	GtkWidget *item;
	GtkWidget *image;
	const gchar *object_path;
	UpDevice *device;
	UpDeviceKind kind_tmp;
	gdouble percentage;

	/* find type */
	for (i=0;i<array->len;i++) {
		device = g_ptr_array_index (array, i);

		/* get device properties */
		g_object_get (device,
			      "kind", &kind_tmp,
			      "percentage", &percentage,
			      "vendor", &vendor,
			      "model", &model,
			      NULL);

		if (kind != kind_tmp)
			continue;

		object_path = up_device_get_object_path (device);
		egg_debug ("adding device %s", object_path);
		added++;

		/* generate the label */
		if ((vendor != NULL && strlen(vendor) != 0) && (model != NULL && strlen(model) != 0)) {
			label = g_strdup_printf ("%s %s (%.1f%%)", vendor, model, percentage);
		}
		else {
			label = g_strdup_printf ("%s (%.1f%%)", gpm_device_kind_to_localised_string (kind, 1), percentage);
		}
		item = gtk_image_menu_item_new_with_label (label);

		/* generate the image */
		icon_name = gpm_upower_get_device_icon (device);
		image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
		gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM (item), TRUE);

		/* set callback and add the menu */
		g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (gpm_tray_icon_show_info_cb), icon);
		g_object_set_data (G_OBJECT (item), "object-path", (gpointer) object_path);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

		g_free (icon_name);
		g_free (label);
		g_free (vendor);
		g_free (model);
	}
	return added;
}

/**
 * gpm_tray_icon_add_primary_device:
 **/
static void
gpm_tray_icon_add_primary_device (GpmTrayIcon *icon, GtkMenu *menu, UpDevice *device)
{
	GtkWidget *item;
	gchar *time_str;
	gchar *string;
	gint64 time_to_empty = 0;

	/* get details */
	g_object_get (device,
		      "time-to-empty", &time_to_empty,
		      NULL);

	/* convert time to string */
	time_str = gpm_get_timestring (time_to_empty);

	/* TRANSLATORS: % is a timestring, e.g. "6 hours 10 minutes" */
	string = g_strdup_printf (_("%s remaining"), time_str);
	item = gtk_image_menu_item_new_with_label (string);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	g_free (time_str);
	g_free (string);
}

/**
 * gpm_tray_icon_create_menu:
 *
 * Create the popup menu.
 **/
static GtkMenu *
gpm_tray_icon_create_menu (GpmTrayIcon *icon)
{
	GtkMenu *menu = (GtkMenu*) gtk_menu_new ();
	GtkWidget *item;
	GtkWidget *image;
	guint dev_cnt = 0;
	GPtrArray *array;
	UpDevice *device = NULL;

	/* show the primary device time remaining */
	device = gpm_engine_get_primary_device (icon->priv->engine);
	if (device != NULL) {
		gpm_tray_icon_add_primary_device (icon, menu, device);
		item = gtk_separator_menu_item_new ();
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	}

	/* add all device types to the drop down menu */
	array = gpm_engine_get_devices (icon->priv->engine);
	dev_cnt += gpm_tray_icon_add_device (icon, menu, array, UP_DEVICE_KIND_BATTERY);
	dev_cnt += gpm_tray_icon_add_device (icon, menu, array, UP_DEVICE_KIND_UPS);
	dev_cnt += gpm_tray_icon_add_device (icon, menu, array, UP_DEVICE_KIND_MOUSE);
	dev_cnt += gpm_tray_icon_add_device (icon, menu, array, UP_DEVICE_KIND_KEYBOARD);
	dev_cnt += gpm_tray_icon_add_device (icon, menu, array, UP_DEVICE_KIND_PDA);
	dev_cnt += gpm_tray_icon_add_device (icon, menu, array, UP_DEVICE_KIND_PHONE);
	dev_cnt += gpm_tray_icon_add_device (icon, menu, array, UP_DEVICE_KIND_MEDIA_PLAYER);
	dev_cnt += gpm_tray_icon_add_device (icon, menu, array, UP_DEVICE_KIND_TABLET);
	dev_cnt += gpm_tray_icon_add_device (icon, menu, array, UP_DEVICE_KIND_COMPUTER);
	g_ptr_array_unref (array);

	/* skip for things like live-cd's and GDM */
	if (!icon->priv->show_actions)
		goto skip_prefs;

	/* only do the separator if we have at least one device */
	if (dev_cnt != 0) {
		item = gtk_separator_menu_item_new ();
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	}

	/* preferences */
	item = gtk_image_menu_item_new_with_mnemonic (_("_Preferences"));
	image = gtk_image_new_from_icon_name ("preferences-system", GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (gpm_tray_icon_show_preferences_cb), icon);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	
	/*Set up custom panel menu theme support-gtk3 only */
	GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (menu));
	/* Fix any failures of compiz/other wm's to communicate with gtk for transparency in menu theme */
	GdkScreen *screen = gtk_widget_get_screen(GTK_WIDGET(toplevel));
	GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
	gtk_widget_set_visual(GTK_WIDGET(toplevel), visual);
	/* Set menu and its toplevel window to follow panel theme */
	GtkStyleContext *context;
	context = gtk_widget_get_style_context (GTK_WIDGET(toplevel));
	gtk_style_context_add_class(context,"gnome-panel-menu-bar");
	gtk_style_context_add_class(context,"mate-panel-menu-bar");

	/* about */
	item = gtk_image_menu_item_new_from_stock (GTK_STOCK_ABOUT, NULL);
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (gpm_tray_icon_show_about_cb), icon);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

skip_prefs:
	if (device != NULL)
		g_object_unref (device);
	return menu;
}

/**
 * gpm_tray_icon_popup_cleared_cd:
 * @widget: The popup Gtkwidget
 *
 * We have to re-enable the tooltip when the popup is removed
 **/
static void
gpm_tray_icon_popup_cleared_cd (GtkWidget *widget, GpmTrayIcon *icon)
{
	g_return_if_fail (GPM_IS_TRAY_ICON (icon));
	egg_debug ("clear tray");
	g_object_ref_sink (widget);
	g_object_unref (widget);
}

/**
 * gpm_tray_icon_popup_menu:
 *
 * Display the popup menu.
 **/
static void
gpm_tray_icon_popup_menu (GpmTrayIcon *icon, guint32 timestamp)
{
	GtkMenu *menu;

	menu = gpm_tray_icon_create_menu (icon);

	/* show the menu */
	gtk_widget_show_all (GTK_WIDGET (menu));
	gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
			gtk_status_icon_position_menu, icon->priv->status_icon,
			1, timestamp);

	g_signal_connect (GTK_WIDGET (menu), "hide",
			  G_CALLBACK (gpm_tray_icon_popup_cleared_cd), icon);
}

/**
 * gpm_tray_icon_popup_menu_cb:
 *
 * Display the popup menu.
 **/
static void
gpm_tray_icon_popup_menu_cb (GtkStatusIcon *status_icon, guint button, guint32 timestamp, GpmTrayIcon *icon)
{
	egg_debug ("icon right clicked");
	gpm_tray_icon_popup_menu (icon, timestamp);
}


/**
 * gpm_tray_icon_activate_cb:
 * @button: Which buttons are pressed
 *
 * Callback when the icon is clicked
 **/
static void
gpm_tray_icon_activate_cb (GtkStatusIcon *status_icon, GpmTrayIcon *icon)
{
	egg_debug ("icon left clicked");
	gpm_tray_icon_popup_menu (icon, gtk_get_current_event_time());
}

/**
 * gpm_tray_icon_settings_changed_cb:
 *
 * We might have to do things when the settings change; do them here.
 **/
static void
gpm_tray_icon_settings_changed_cb (GSettings *settings, const gchar *key, GpmTrayIcon *icon)
{
	gboolean allowed_in_menu;

	if (g_strcmp0 (key, GPM_SETTINGS_SHOW_ACTIONS) == 0) {
		allowed_in_menu = g_settings_get_boolean (settings, key);
		gpm_tray_icon_enable_actions (icon, allowed_in_menu);
	}
}

/**
 * gpm_tray_icon_init:
 *
 * Initialise the tray object
 **/
static void
gpm_tray_icon_init (GpmTrayIcon *icon)
{
	gboolean allowed_in_menu;

	icon->priv = GPM_TRAY_ICON_GET_PRIVATE (icon);

	icon->priv->engine = gpm_engine_new ();

	icon->priv->settings = g_settings_new (GPM_SETTINGS_SCHEMA);
	g_signal_connect (icon->priv->settings, "changed",
			  G_CALLBACK (gpm_tray_icon_settings_changed_cb), icon);

	icon->priv->status_icon = gtk_status_icon_new ();
	gpm_tray_icon_show (icon, FALSE);
	g_signal_connect_object (G_OBJECT (icon->priv->status_icon),
				 "popup_menu",
				 G_CALLBACK (gpm_tray_icon_popup_menu_cb),
				 icon, 0);
	g_signal_connect_object (G_OBJECT (icon->priv->status_icon),
				 "activate",
				 G_CALLBACK (gpm_tray_icon_activate_cb),
				 icon, 0);

	allowed_in_menu = g_settings_get_boolean (icon->priv->settings, GPM_SETTINGS_SHOW_ACTIONS);
	gpm_tray_icon_enable_actions (icon, allowed_in_menu);
}

/**
 * gpm_tray_icon_finalize:
 * @object: This TrayIcon class instance
 **/
static void
gpm_tray_icon_finalize (GObject *object)
{
	GpmTrayIcon *tray_icon;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_TRAY_ICON (object));

	tray_icon = GPM_TRAY_ICON (object);

	g_object_unref (tray_icon->priv->status_icon);
	g_object_unref (tray_icon->priv->engine);
	g_return_if_fail (tray_icon->priv != NULL);

	G_OBJECT_CLASS (gpm_tray_icon_parent_class)->finalize (object);
}

/**
 * gpm_tray_icon_new:
 * Return value: A new TrayIcon object.
 **/
GpmTrayIcon *
gpm_tray_icon_new (void)
{
	GpmTrayIcon *tray_icon;
	tray_icon = g_object_new (GPM_TYPE_TRAY_ICON, NULL);
	return GPM_TRAY_ICON (tray_icon);
}

