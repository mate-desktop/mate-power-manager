/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * MATE Power Manager Power Profiles Applet
 * Copyright (C) 2025 MATE Developers
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
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mate-panel-applet.h>
#include <gtk/gtk.h>
#include <glib-object.h>
#include <glib/gi18n.h>
#include <dbus/dbus-glib.h>

#include "gpm-common.h"

#define GPM_TYPE_POWER_PROFILES_APPLET		(gpm_power_profiles_applet_get_type ())
#define GPM_POWER_PROFILES_APPLET(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_POWER_PROFILES_APPLET, GpmPowerProfilesApplet))
#define GPM_POWER_PROFILES_APPLET_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_POWER_PROFILES_APPLET, GpmPowerProfilesAppletClass))
#define GPM_IS_POWER_PROFILES_APPLET(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_POWER_PROFILES_APPLET))
#define GPM_IS_POWER_PROFILES_APPLET_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_POWER_PROFILES_APPLET))
#define GPM_POWER_PROFILES_APPLET_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_POWER_PROFILES_APPLET, GpmPowerProfilesAppletClass))

typedef struct{
	MatePanelApplet parent;
	/* applet state */
	gchar *power_profile;
	/* the icon */
	GtkWidget *image;
	/* connection to g-p-m */
	GDBusProxy *proxy;
	GDBusConnection *connection;
	guint bus_watch_id;
	/* a cache for panel size */
	gint size;
} GpmPowerProfilesApplet;

typedef struct{
	MatePanelAppletClass	parent_class;
} GpmPowerProfilesAppletClass;

GType                gpm_power_profiles_applet_get_type  (void);

#define PPD_DBUS_SERVICE	"org.freedesktop.UPower.PowerProfiles"
#define PPD_DBUS_PATH		"/org/freedesktop/UPower/PowerProfiles"
#define PPD_DBUS_INTERFACE	"org.freedesktop.UPower.PowerProfiles"

G_DEFINE_TYPE (GpmPowerProfilesApplet, gpm_power_profiles_applet, PANEL_TYPE_APPLET)

static void	gpm_applet_update_icon		(GpmPowerProfilesApplet *applet);
static void	gpm_applet_size_allocate_cb     (GtkWidget *widget, GdkRectangle *allocation);
static void	gpm_applet_update_tooltip	(GpmPowerProfilesApplet *applet);
static gboolean	gpm_applet_click_cb		(GpmPowerProfilesApplet *applet, GdkEventButton *event);
static void	gpm_applet_dialog_about_cb	(GtkAction *action, gpointer data);
static gboolean	gpm_applet_cb		        (MatePanelApplet *_applet, const gchar *iid, gpointer data);
static void	gpm_applet_destroy_cb		(GtkWidget *widget);

#define GPM_POWER_PROFILES_APPLET_ID		        "PowerProfilesApplet"
#define GPM_POWER_PROFILES_APPLET_FACTORY_ID	        "PowerProfilesAppletFactory"
#define GPM_POWER_PROFILES_APPLET_ICON_POWER_SAVER	"gpm-power-profile-power-saver"
#define GPM_POWER_PROFILES_APPLET_ICON_BALANCED		"gpm-power-profile-balanced"
#define GPM_POWER_PROFILES_APPLET_ICON_PERFORMANCE	"gpm-power-profile-performance"
#define GPM_POWER_PROFILES_APPLET_NAME			_("Power Manager Power Profiles Applet")
#define GPM_POWER_PROFILES_APPLET_DESC			_("Allows user to adjust system power profiles.")
#define MATE_PANEL_APPLET_VERTICAL(p)					\
	 (((p) == MATE_PANEL_APPLET_ORIENT_LEFT) || ((p) == MATE_PANEL_APPLET_ORIENT_RIGHT))

/**
 * gpm_applet_get_power_profile:
 * @applet: Power profiles applet instance
 *
 * Fetches the active power profile from DBUS.
 **/
static void
gpm_applet_get_power_profile (GpmPowerProfilesApplet *applet)
{
	GError  *error = NULL;
	GVariant *result;
	GVariant *variant;

	if (applet->connection == NULL) {
		g_warning ("not connected");
		return;
	}

	result = g_dbus_connection_call_sync (applet->connection,
					     PPD_DBUS_SERVICE,
					     PPD_DBUS_PATH,
					     "org.freedesktop.DBus.Properties",
					     "Get",
					     g_variant_new("(ss)", PPD_DBUS_INTERFACE, "ActiveProfile"),
					     NULL,
					     G_DBUS_CALL_FLAGS_NONE, -1, NULL,
					     &error);

	if (error != NULL) {
		g_warning ("Failed to get current profile: %s\n", error->message);
		g_clear_error(&error);
		return;
	}

	g_variant_get(result, "(v)", &variant);
	applet->power_profile = g_strdup (g_variant_get_string (variant, NULL));

	g_variant_unref (variant);
	g_variant_unref (result);
}

/**
 * gpm_applet_set_power_profile:
 * @applet: Power profiles applet instance
 * @power_profile: The new profile to set (power-saver, balanced, performance)
 *
 * Sets the new active power profile using DBUS.
 **/
static void
gpm_applet_set_power_profile (GpmPowerProfilesApplet *applet,
			      const gchar	     *power_profile)
{
	GError *error = NULL;
	GVariant *value;
	GVariant *result;

	if (applet->connection == NULL) {
		g_warning ("not connected");
		return;
	}

	value = g_variant_new_string (power_profile);

	result = g_dbus_connection_call_sync (applet->connection,
					     PPD_DBUS_SERVICE,
					     PPD_DBUS_PATH,
					     "org.freedesktop.DBus.Properties",
					     "Set",
					     g_variant_new("(ssv)", PPD_DBUS_INTERFACE, "ActiveProfile", value),
					     NULL,
					     G_DBUS_CALL_FLAGS_NONE, -1, NULL,
					     &error);

	if (error != NULL) {
		applet->power_profile = g_strdup_printf ("%s", error->message);
		gpm_applet_update_tooltip (applet);
		g_warning ("Failed to set property: %s\n", error->message);
		g_clear_error (&error);
	} else {
		g_debug ("Power profile set to: %s\n", power_profile);
		g_variant_unref (result);
	}
}

static void
gpm_applet_set_power_profile_power_saver (GtkMenuItem *item, gpointer data)
{
	GpmPowerProfilesApplet *applet = data;

	gpm_applet_set_power_profile (applet, "power-saver");
}

static void
gpm_applet_set_power_profile_balanced (GtkMenuItem *item, gpointer data)
{
	GpmPowerProfilesApplet *applet = data;

	gpm_applet_set_power_profile (applet, "balanced");
}

static void
gpm_applet_set_power_profile_performance (GtkMenuItem *item, gpointer data)
{
	GpmPowerProfilesApplet *applet = data;

	gpm_applet_set_power_profile (applet, "performance");
}

/**
 * gpm_applet_update_icon:
 * @applet: Power profiles applet instance
 *
 * sets an icon from stock
 **/
static void
gpm_applet_update_icon (GpmPowerProfilesApplet *applet)
{
	const gchar *icon;

	if (applet->proxy == NULL || applet->power_profile == NULL) {
		icon = GPM_POWER_PROFILES_APPLET_ICON_BALANCED;
	} else if (g_strcmp0 (applet->power_profile, "performance") == 0) {
		icon = GPM_POWER_PROFILES_APPLET_ICON_PERFORMANCE;
	} else if (g_strcmp0 (applet->power_profile, "power-saver") == 0) {
		icon = GPM_POWER_PROFILES_APPLET_ICON_POWER_SAVER;
	} else {
		icon = GPM_POWER_PROFILES_APPLET_ICON_BALANCED;
	}
	gtk_image_set_from_icon_name (GTK_IMAGE(applet->image),
				      icon,
				      GTK_ICON_SIZE_BUTTON);
}

/**
 * gpm_applet_update_tooltip:
 * @applet: Power profiles applet instance
 *
 * sets tooltip's content (Power Saver, Balanced, or Performance)
 **/
static void
gpm_applet_update_tooltip (GpmPowerProfilesApplet *applet)
{
	const gchar *buf;
	if (applet->connection == NULL) {
		buf = _("Cannot connect to DBUS daemon");
	} else if (applet->proxy == NULL) {
		buf = _("Cannot connect to mate-power-manager");
	} else {
		if (g_strcmp0 (applet->power_profile, "performance") == 0) {
			buf = _("Active Profile: Performance");
		} else if (g_strcmp0 (applet->power_profile, "power-saver") == 0) {
			buf = _("Active Profile: Power Saver");
		} else {
			buf = _("Active Profile: Balanced");
		}
	}
	gtk_widget_set_tooltip_text (GTK_WIDGET(applet), buf);
}

/**
 * gpm_applet_create_menu:
 *
 * Create the popup menu.
 **/
static GtkMenu *
gpm_applet_create_menu (GpmPowerProfilesApplet *applet)
{
	GtkMenu *menu = (GtkMenu*) gtk_menu_new ();
	GtkWidget *item;
	GtkWidget *image;
	GtkStyleContext *context;
	GtkWidget       *toplevel;
	GdkScreen       *screen;
	GdkVisual       *visual;

	/* Power Saver */
	item = gtk_image_menu_item_new_with_mnemonic (_("Power _Saver"));
	image = gtk_image_new_from_icon_name (GPM_POWER_PROFILES_APPLET_ICON_POWER_SAVER, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (gpm_applet_set_power_profile_power_saver), applet);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	/* Balanced */
	item = gtk_image_menu_item_new_with_mnemonic (_("_Balanced"));
	image = gtk_image_new_from_icon_name (GPM_POWER_PROFILES_APPLET_ICON_BALANCED, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (gpm_applet_set_power_profile_balanced), applet);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	/* Performance */
	item = gtk_image_menu_item_new_with_mnemonic (_("_Performance"));
	image = gtk_image_new_from_icon_name (GPM_POWER_PROFILES_APPLET_ICON_PERFORMANCE, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (gpm_applet_set_power_profile_performance), applet);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	/*Set up custom panel menu theme support-gtk3 only */
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (menu));
	/* Fix any failures of compiz/other wm's to communicate with gtk for transparency in menu theme */
	screen = gtk_widget_get_screen (GTK_WIDGET(toplevel));
	visual = gdk_screen_get_rgba_visual (screen);
	gtk_widget_set_visual (GTK_WIDGET (toplevel), visual);
	/* Set menu and its toplevel window to follow panel theme */
	context = gtk_widget_get_style_context (GTK_WIDGET(toplevel));
	gtk_style_context_add_class (context,"gnome-panel-menu-bar");
	gtk_style_context_add_class (context,"mate-panel-menu-bar");

	return menu;
}

/**
 * gpm_applet_popup_cleared_cb:
 * @widget: The popup Gtkwidget
 *
 * We have to re-enable the tooltip when the popup is removed
 **/
static void
gpm_applet_popup_cleared_cb (GtkWidget *widget, GpmPowerProfilesApplet *applet)
{
	g_return_if_fail (GPM_IS_POWER_PROFILES_APPLET (applet));
	g_object_ref_sink (widget);
	g_object_unref (widget);
}

/**
 * gpm_applet_popup_menu:
 *
 * Display the popup menu.
 **/
static void
gpm_applet_popup_menu (GpmPowerProfilesApplet *applet, guint32 timestamp)
{
	GtkMenu *menu;

	menu = gpm_applet_create_menu (applet);

	/* show the menu */
	gtk_widget_show_all (GTK_WIDGET (menu));
	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL,
			applet, 1, timestamp);

	g_signal_connect (GTK_WIDGET (menu), "hide",
			  G_CALLBACK (gpm_applet_popup_cleared_cb), applet);
}

/**
 * gpm_applet_click_cb:
 * @applet: Power profiles applet instance
 *
 * pops and unpops
 **/
static gboolean
gpm_applet_click_cb (GpmPowerProfilesApplet *applet, GdkEventButton *event)
{
	/* react only to left mouse button */
	if (event->button != 1) {
		return FALSE;
	}

	gpm_applet_popup_menu (applet, gtk_get_current_event_time());

	gpm_applet_get_power_profile (applet);
	gpm_applet_update_icon (applet);
	gpm_applet_update_tooltip (applet);

	return TRUE;
}

/**
 * gpm_applet_dialog_about_cb:
 *
 * displays about dialog
 **/
static void
gpm_applet_dialog_about_cb (GtkAction *action, gpointer data)
{
	static const gchar *authors[] = {
		"Victor Kareh <vkareh@redhat.com>",
		NULL
	};

	const char *documenters [] = {
		NULL
	};

	const char *license[] = {
		 N_("Power Manager is free software; you can redistribute it and/or "
		   "modify it under the terms of the GNU General Public License "
		   "as published by the Free Software Foundation; either version 2 "
		   "of the License, or (at your option) any later version."),

		 N_("Power Manager is distributed in the hope that it will be useful, "
		   "but WITHOUT ANY WARRANTY; without even the implied warranty of "
		   "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
		   "GNU General Public License for more details.") ,

		 N_("You should have received a copy of the GNU General Public License "
		   "along with this program; if not, write to the Free Software "
		   "Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA "
		   "02110-1301, USA.")
	};

	char *license_trans;

	license_trans = g_strjoin("\n\n", _(license[0]), _(license[1]), _(license[2]), NULL);

	gtk_show_about_dialog (NULL,
	                       "program-name", GPM_POWER_PROFILES_APPLET_NAME,
	                       "version", VERSION,
	                       "title", _("About Power Manager Power Profiles Applet"),
	                       "comments", GPM_POWER_PROFILES_APPLET_DESC,
	                       "copyright", _("Copyright \xC2\xA9 2025 MATE developers"),
	                       "icon-name", "mate-power-profiles-applet",
	                       "logo-icon-name", "mate-power-profiles-applet",
	                       "license", license_trans,
	                       "authors", authors,
	                       "documenters", documenters,
	                       "translator-credits", _("translator-credits"),
	                       "wrap-license", TRUE,
	                       "website", PACKAGE_URL,
	                       NULL);

	g_free (license_trans);
}

/**
 * gpm_applet_help_cb:
 *
 * open gpm help
 **/
static void
gpm_applet_help_cb (GtkAction *action, gpointer data)
{
	gpm_help_display ("applets-general#applets-power-profiles");
}

/**
 * gpm_applet_destroy_cb:
 * @widget: Class instance to destroy
 **/
static void
gpm_applet_destroy_cb (GtkWidget *widget)
{
	GpmPowerProfilesApplet *applet = GPM_POWER_PROFILES_APPLET(widget);

	g_bus_unwatch_name (applet->bus_watch_id);
}

/**
 * gpm_power_profiles_applet_class_init:
 * @klass: Class instance
 **/
static void
gpm_power_profiles_applet_class_init (GpmPowerProfilesAppletClass *class)
{
	/* nothing to do here */
}

static void
gpm_applet_properties_changed_cb (GDBusProxy *session,
				  GVariant   *changed,
				  char      **invalidated,
				  gpointer    data)
{
	GVariant *v;
	GpmPowerProfilesApplet *applet = data;

	v = g_variant_lookup_value (changed, "ActiveProfile", G_VARIANT_TYPE_STRING);
	if (v) {
		applet->power_profile = g_variant_get_string (v, NULL);
		g_debug ("Received system active power profile: %s", applet->power_profile);

		gpm_applet_update_tooltip (applet);
		gpm_applet_update_icon (applet);

		g_variant_unref (v);
	}
}

/**
 * gpm_power_profiles_applet_dbus_connect:
 **/
static gboolean
gpm_power_profiles_applet_dbus_connect (GpmPowerProfilesApplet *applet)
{
	GError *error = NULL;

	if (applet->connection == NULL) {
		g_debug ("get connection\n");
		g_clear_error (&error);
		applet->connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
		if (!applet->connection) {
			g_warning ("Could not connect to DBUS daemon: %s", error->message);
			g_clear_error(&error);
			applet->connection = NULL;
			return FALSE;
		}
	}
	if (applet->proxy == NULL) {
		g_debug ("get proxy\n");
		g_clear_error (&error);

		applet->proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
							       G_DBUS_PROXY_FLAGS_NONE,
							       NULL,
							       PPD_DBUS_SERVICE,
							       PPD_DBUS_PATH,
							       PPD_DBUS_INTERFACE,
							       NULL,
							       &error);
		if (error != NULL) {
			g_warning ("Cannot connect, maybe the daemon is not running: %s\n", error->message);
			g_error_free (error);
			applet->proxy = NULL;
			return FALSE;
		}

		g_signal_connect (applet->proxy, "g-properties-changed",
				  G_CALLBACK (gpm_applet_properties_changed_cb),
				  applet);
		}
	return TRUE;
}

/**
 * gpm_power_profiles_applet_dbus_disconnect:
 **/
static gboolean
gpm_power_profiles_applet_dbus_disconnect (GpmPowerProfilesApplet *applet)
{
	if (applet->proxy != NULL) {
		g_debug ("removing proxy\n");
		g_object_unref (applet->proxy);
		applet->proxy = NULL;
		/* we have no power profile selected, these are not persistent across reboots */
		applet->power_profile = g_strdup("unknown");
	}
	return TRUE;
}

/**
 * gpm_power_profiles_applet_name_appeared_cb:
 **/
static void
gpm_power_profiles_applet_name_appeared_cb (GDBusConnection *connection,
				     const gchar *name,
				     const gchar *name_owner,
				     GpmPowerProfilesApplet *applet)
{
	gpm_power_profiles_applet_dbus_connect (applet);
	gpm_applet_get_power_profile (applet);
	gpm_applet_update_tooltip (applet);
	gpm_applet_update_icon (applet);;
}

/**
 * gpm_power_profiles_applet_name_vanished_cb:
 **/
static void
gpm_power_profiles_applet_name_vanished_cb (GDBusConnection *connection,
				     const gchar *name,
				     GpmPowerProfilesApplet *applet)
{
	gpm_power_profiles_applet_dbus_disconnect (applet);
	gpm_applet_get_power_profile (applet);
	gpm_applet_update_tooltip (applet);
	gpm_applet_update_icon (applet);
}

/**
 * gpm_applet_size_allocate_cb:
 * @applet: Power Profiles applet instance
 *
 * resize icon when panel size changed
 **/
static void
gpm_applet_size_allocate_cb (GtkWidget    *widget,
                             GdkRectangle *allocation)
{
	GpmPowerProfilesApplet *applet = GPM_POWER_PROFILES_APPLET (widget);
	int               size = 0;

	switch (mate_panel_applet_get_orient (MATE_PANEL_APPLET (applet))) {
		case MATE_PANEL_APPLET_ORIENT_LEFT:
		case MATE_PANEL_APPLET_ORIENT_RIGHT:
			size = allocation->width;
			break;

		case MATE_PANEL_APPLET_ORIENT_UP:
		case MATE_PANEL_APPLET_ORIENT_DOWN:
			size = allocation->height;
			break;
		default:
			break;
	}

	/* Scale to the actual size of the applet, don't quantize to original icon size */
	/* GtkImage already contains a check to do nothing if it's the same */
	gtk_image_set_pixel_size (GTK_IMAGE(applet->image), size);
}

/**
 * gpm_power_profiles_applet_init:
 * @applet: Power Profiles applet instance
 **/
static void
gpm_power_profiles_applet_init (GpmPowerProfilesApplet *applet)
{
	/* initialize fields */
	applet->image = NULL;
	applet->power_profile = g_strdup("unknown");
	applet->connection = NULL;
	applet->proxy = NULL;

	/* Add application specific icons to search path */
	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
                                           GPM_ICONS_DATA);

	/* monitor the daemon */
	applet->bus_watch_id =
		g_bus_watch_name (G_BUS_TYPE_SYSTEM,
				  PPD_DBUS_SERVICE,
				  G_BUS_NAME_WATCHER_FLAGS_NONE,
				  (GBusNameAppearedCallback) gpm_power_profiles_applet_name_appeared_cb,
				  (GBusNameVanishedCallback) gpm_power_profiles_applet_name_vanished_cb,
				  applet, NULL);

	/* prepare */
	mate_panel_applet_set_flags (MATE_PANEL_APPLET (applet), MATE_PANEL_APPLET_EXPAND_MINOR);
	applet->image = gtk_image_new();
	gtk_container_add (GTK_CONTAINER (applet), applet->image);

	/* set appropriate size and load icon accordingly */
	gtk_widget_queue_draw (GTK_WIDGET (applet));

	/* show */
	gtk_widget_show_all (GTK_WIDGET(applet));

	/* connect */
	g_signal_connect (G_OBJECT(applet), "button-press-event",
			  G_CALLBACK(gpm_applet_click_cb), NULL);

	g_signal_connect (G_OBJECT(applet), "size-allocate",
			  G_CALLBACK(gpm_applet_size_allocate_cb), NULL);

	g_signal_connect (G_OBJECT(applet), "destroy",
			  G_CALLBACK(gpm_applet_destroy_cb), NULL);
}

/**
 * gpm_applet_cb:
 * @_applet: GpmPowerProfilesApplet instance created by the applet factory
 * @iid: Applet id
 *
 * the function called by libmate-panel-applet factory after creation
 **/
static gboolean
gpm_applet_cb (MatePanelApplet *_applet, const gchar *iid, gpointer data)
{
	GpmPowerProfilesApplet *applet = GPM_POWER_PROFILES_APPLET(_applet);
	GtkActionGroup *action_group;

	static const GtkActionEntry menu_actions [] = {
		{ "About", "help-about", N_("_About"),
		  NULL, NULL,
		  G_CALLBACK (gpm_applet_dialog_about_cb) },
		{ "Help", "help-browser", N_("_Help"),
		  NULL, NULL,
		  G_CALLBACK (gpm_applet_help_cb) }
	};

	if (strcmp (iid, GPM_POWER_PROFILES_APPLET_ID) != 0) {
		return FALSE;
	}

	action_group = gtk_action_group_new ("Power Profiles Applet Actions");
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (action_group,
				      menu_actions,
				      G_N_ELEMENTS (menu_actions),
				      applet);
	mate_panel_applet_setup_menu_from_file (MATE_PANEL_APPLET (applet),
	                                        POWER_PROFILES_MENU_UI_DIR "/power-profiles-applet-menu.xml",
	                                        action_group);
	g_object_unref (action_group);

	return TRUE;
}

/**
 * this generates a main with a applet factory
 **/
#ifdef APPLETS_INPROCESS
MATE_PANEL_APPLET_IN_PROCESS_FACTORY
 (/* the factory iid */
 GPM_POWER_PROFILES_APPLET_FACTORY_ID,
 /* generates power profiles applets instead of regular mate applets  */
 GPM_TYPE_POWER_PROFILES_APPLET,
 /* the applet name */
 "PowerProfilesApplet",
 /* our callback (with no user data) */
 gpm_applet_cb, NULL)
#else
MATE_PANEL_APPLET_OUT_PROCESS_FACTORY
 (/* the factory iid */
 GPM_POWER_PROFILES_APPLET_FACTORY_ID,
 /* generates power profiles applets instead of regular mate applets  */
 GPM_TYPE_POWER_PROFILES_APPLET,
 /* the applet name */
 "PowerProfilesApplet",
 /* our callback (with no user data) */
 gpm_applet_cb, NULL)
#endif
