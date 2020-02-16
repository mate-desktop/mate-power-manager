/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * MATE Power Manager Inhibit Applet
 * Copyright (C) 2006 Benjamin Canou <bookeldor@gmail.com>
 * Copyright (C) 2006-2009 Richard Hughes <richard@hughsie.com>
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

#include "egg-debug.h"
#include "gpm-common.h"

#define GPM_TYPE_INHIBIT_APPLET		(gpm_inhibit_applet_get_type ())
#define GPM_INHIBIT_APPLET(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_TYPE_INHIBIT_APPLET, GpmInhibitApplet))
#define GPM_INHIBIT_APPLET_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GPM_TYPE_INHIBIT_APPLET, GpmInhibitAppletClass))
#define GPM_IS_INHIBIT_APPLET(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_TYPE_INHIBIT_APPLET))
#define GPM_IS_INHIBIT_APPLET_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GPM_TYPE_INHIBIT_APPLET))
#define GPM_INHIBIT_APPLET_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GPM_TYPE_INHIBIT_APPLET, GpmInhibitAppletClass))

typedef struct{
	MatePanelApplet parent;
	/* applet state */
	guint cookie;
	/* the icon */
	GtkWidget *image;
	/* connection to g-p-m */
	DBusGProxy *proxy;
	DBusGConnection *connection;
	guint bus_watch_id;
	guint level;
	/* a cache for panel size */
	gint size;
} GpmInhibitApplet;

typedef struct{
	MatePanelAppletClass	parent_class;
} GpmInhibitAppletClass;

GType                gpm_inhibit_applet_get_type  (void);

#define GS_DBUS_SERVICE		"org.gnome.SessionManager"
#define GS_DBUS_PATH		"/org/gnome/SessionManager"
#define GS_DBUS_INTERFACE	"org.gnome.SessionManager"

G_DEFINE_TYPE (GpmInhibitApplet, gpm_inhibit_applet, PANEL_TYPE_APPLET)

static void	gpm_applet_update_icon		(GpmInhibitApplet *applet);
static void	gpm_applet_size_allocate_cb     (GtkWidget *widget, GdkRectangle *allocation);;
static void	gpm_applet_update_tooltip	(GpmInhibitApplet *applet);
static gboolean	gpm_applet_click_cb		(GpmInhibitApplet *applet, GdkEventButton *event);
static void	gpm_applet_dialog_about_cb	(GtkAction *action, gpointer data);
static gboolean	gpm_applet_cb		        (MatePanelApplet *_applet, const gchar *iid, gpointer data);
static void	gpm_applet_destroy_cb		(GtkWidget *widget);

#define GPM_INHIBIT_APPLET_ID		        "InhibitApplet"
#define GPM_INHIBIT_APPLET_FACTORY_ID	        "InhibitAppletFactory"
#define GPM_INHIBIT_APPLET_ICON_INHIBIT		"gpm-inhibit"
#define GPM_INHIBIT_APPLET_ICON_INVALID		"gpm-inhibit-invalid"
#define GPM_INHIBIT_APPLET_ICON_UNINHIBIT	"gpm-hibernate"
#define GPM_INHIBIT_APPLET_NAME			_("Power Manager Inhibit Applet")
#define GPM_INHIBIT_APPLET_DESC			_("Allows user to inhibit automatic power saving.")
#define MATE_PANEL_APPLET_VERTICAL(p)					\
	 (((p) == MATE_PANEL_APPLET_ORIENT_LEFT) || ((p) == MATE_PANEL_APPLET_ORIENT_RIGHT))


/** cookie is returned as an unsigned integer */
static gboolean
gpm_applet_inhibit (GpmInhibitApplet *applet,
		    const gchar     *appname,
		    const gchar     *reason,
		    guint           *cookie)
{
	GError  *error = NULL;
	gboolean ret;

	g_return_val_if_fail (cookie != NULL, FALSE);

	if (applet->proxy == NULL) {
		egg_warning ("not connected\n");
		return FALSE;
	}

	ret = dbus_g_proxy_call (applet->proxy, "Inhibit", &error,
				 G_TYPE_STRING, appname,
				 G_TYPE_UINT, 0, /* xid */
				 G_TYPE_STRING, reason,
				 G_TYPE_UINT, 1+2+4+8, /* logoff, switch, suspend, and idle */
				 G_TYPE_INVALID,
				 G_TYPE_UINT, cookie,
				 G_TYPE_INVALID);
	if (error) {
		g_debug ("ERROR: %s", error->message);
		g_error_free (error);
		*cookie = 0;
	}
	if (!ret) {
		/* abort as the DBUS method failed */
		g_warning ("Inhibit failed!");
	}

	return ret;
}

static gboolean
gpm_applet_uninhibit (GpmInhibitApplet *applet,
		      guint            cookie)
{
	GError *error = NULL;
	gboolean ret;

	if (applet->proxy == NULL) {
		egg_warning ("not connected");
		return FALSE;
	}

	ret = dbus_g_proxy_call (applet->proxy, "Uninhibit", &error,
				 G_TYPE_UINT, cookie,
				 G_TYPE_INVALID,
				 G_TYPE_INVALID);
	if (error) {
		g_debug ("ERROR: %s", error->message);
		g_error_free (error);
	}
	if (!ret) {
		/* abort as the DBUS method failed */
		g_warning ("Uninhibit failed!");
	}

	return ret;
}

/**
 * gpm_applet_update_icon:
 * @applet: Inhibit applet instance
 *
 * sets an icon from stock
 **/
static void
gpm_applet_update_icon (GpmInhibitApplet *applet)
{
	const gchar *icon;

	/* get icon */
	if (applet->proxy == NULL) {
		icon = GPM_INHIBIT_APPLET_ICON_INVALID;
	} else if (applet->cookie > 0) {
		icon = GPM_INHIBIT_APPLET_ICON_INHIBIT;
	} else {
		icon = GPM_INHIBIT_APPLET_ICON_UNINHIBIT;
	}
	gtk_image_set_from_icon_name (GTK_IMAGE(applet->image),
				      icon,
				      GTK_ICON_SIZE_BUTTON);
}

/**
 * gpm_applet_size_allocate_cb:
 * @applet: Inhibit applet instance
 *
 * resize icon when panel size changed
 **/
static void
gpm_applet_size_allocate_cb (GtkWidget    *widget,
                             GdkRectangle *allocation)
{
	GpmInhibitApplet *applet = GPM_INHIBIT_APPLET (widget);
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
	}

	/* Scale to the actual size of the applet, don't quantize to original icon size */
	/* GtkImage already contains a check to do nothing if it's the same */
	gtk_image_set_pixel_size (GTK_IMAGE(applet->image), size);
}


/**
 * gpm_applet_update_tooltip:
 * @applet: Inhibit applet instance
 *
 * sets tooltip's content (percentage or disabled)
 **/
static void
gpm_applet_update_tooltip (GpmInhibitApplet *applet)
{
	const gchar *buf;
	if (applet->proxy == NULL) {
		buf = _("Cannot connect to mate-power-manager");
	} else {
		if (applet->cookie > 0) {
			buf = _("Automatic sleep inhibited");
		} else {
			buf = _("Automatic sleep enabled");
		}
	}
	gtk_widget_set_tooltip_text (GTK_WIDGET(applet), buf);
}

/**
 * gpm_applet_click_cb:
 * @applet: Inhibit applet instance
 *
 * pops and unpops
 **/
static gboolean
gpm_applet_click_cb (GpmInhibitApplet *applet, GdkEventButton *event)
{
	/* react only to left mouse button */
	if (event->button != 1) {
		return FALSE;
	}

	if (applet->cookie > 0) {
		g_debug ("uninhibiting %u", applet->cookie);
		gpm_applet_uninhibit (applet, applet->cookie);
		applet->cookie = 0;
	} else {
		g_debug ("inhibiting");
		gpm_applet_inhibit (applet,
					  GPM_INHIBIT_APPLET_NAME,
					  _("Manual inhibit"),
					  &(applet->cookie));
	}
	/* update icon */
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
		"Benjamin Canou <bookeldor@gmail.com>",
		"Richard Hughes <richard@hughsie.com>",
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
	                       "program-name", GPM_INHIBIT_APPLET_NAME,
	                       "version", VERSION,
	                       "title", _("About Power Manager Inhibit Applet"),
	                       "comments", GPM_INHIBIT_APPLET_DESC,
	                       "copyright", _("Copyright \xC2\xA9 2006-2007 Richard Hughes\n"
	                                      "Copyright \xC2\xA9 2011-2020 MATE developers"),
	                       "icon-name", GPM_INHIBIT_APPLET_ICON_INHIBIT,
	                       "logo-icon-name", GPM_INHIBIT_APPLET_ICON_INHIBIT,
	                       "license", license_trans,
	                       "authors", authors,
	                       "documenters", documenters,
	                       "translator-credits", _("translator-credits"),
	                       "wrap-license", TRUE,
	                       "website", "https://mate-desktop.org",
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
	gpm_help_display ("applets-general#applets-inhibit");
}

/**
 * gpm_applet_destroy_cb:
 * @widget: Class instance to destroy
 **/
static void
gpm_applet_destroy_cb (GtkWidget *widget)
{
	GpmInhibitApplet *applet = GPM_INHIBIT_APPLET(widget);

	g_bus_unwatch_name (applet->bus_watch_id);
}

/**
 * gpm_inhibit_applet_class_init:
 * @klass: Class instance
 **/
static void
gpm_inhibit_applet_class_init (GpmInhibitAppletClass *class)
{
	/* nothing to do here */
}


/**
 * gpm_inhibit_applet_dbus_connect:
 **/
gboolean
gpm_inhibit_applet_dbus_connect (GpmInhibitApplet *applet)
{
	GError *error = NULL;

	if (applet->connection == NULL) {
		egg_debug ("get connection\n");
		g_clear_error (&error);
		applet->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
		if (error != NULL) {
			egg_warning ("Could not connect to DBUS daemon: %s", error->message);
			g_error_free (error);
			applet->connection = NULL;
			return FALSE;
		}
	}
	if (applet->proxy == NULL) {
		egg_debug ("get proxy\n");
		g_clear_error (&error);
		applet->proxy = dbus_g_proxy_new_for_name_owner (applet->connection,
							 GS_DBUS_SERVICE,
							 GS_DBUS_PATH,
							 GS_DBUS_INTERFACE,
							 &error);
		if (error != NULL) {
			egg_warning ("Cannot connect, maybe the daemon is not running: %s\n", error->message);
			g_error_free (error);
			applet->proxy = NULL;
			return FALSE;
		}
	}
	return TRUE;
}

/**
 * gpm_inhibit_applet_dbus_disconnect:
 **/
gboolean
gpm_inhibit_applet_dbus_disconnect (GpmInhibitApplet *applet)
{
	if (applet->proxy != NULL) {
		egg_debug ("removing proxy\n");
		g_object_unref (applet->proxy);
		applet->proxy = NULL;
		/* we have no inhibit, these are not persistent across reboots */
		applet->cookie = 0;
	}
	return TRUE;
}

/**
 * gpm_inhibit_applet_name_appeared_cb:
 **/
static void
gpm_inhibit_applet_name_appeared_cb (GDBusConnection *connection,
				     const gchar *name,
				     const gchar *name_owner,
				     GpmInhibitApplet *applet)
{
	gpm_inhibit_applet_dbus_connect (applet);
	gpm_applet_update_tooltip (applet);
	gpm_applet_update_icon (applet);;
}

/**
 * gpm_inhibit_applet_name_vanished_cb:
 **/
void
gpm_inhibit_applet_name_vanished_cb (GDBusConnection *connection,
				     const gchar *name,
				     GpmInhibitApplet *applet)
{
	gpm_inhibit_applet_dbus_disconnect (applet);
	gpm_applet_update_tooltip (applet);
	gpm_applet_update_icon (applet);
}

/**
 * gpm_inhibit_applet_init:
 * @applet: Inhibit applet instance
 **/
static void
gpm_inhibit_applet_init (GpmInhibitApplet *applet)
{
	DBusGConnection *connection;

	/* initialize fields */
	applet->image = NULL;
	applet->cookie = 0;
	applet->connection = NULL;
	applet->proxy = NULL;

	/* Add application specific icons to search path */
	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
                                           GPM_ICONS_DATA);

	/* monitor the daemon */
	applet->bus_watch_id =
		g_bus_watch_name (G_BUS_TYPE_SESSION,
				  GS_DBUS_SERVICE,
				  G_BUS_NAME_WATCHER_FLAGS_NONE,
				  (GBusNameAppearedCallback) gpm_inhibit_applet_name_appeared_cb,
				  (GBusNameVanishedCallback) gpm_inhibit_applet_name_vanished_cb,
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
 * @_applet: GpmInhibitApplet instance created by the applet factory
 * @iid: Applet id
 *
 * the function called by libmate-panel-applet factory after creation
 **/
static gboolean
gpm_applet_cb (MatePanelApplet *_applet, const gchar *iid, gpointer data)
{
	GpmInhibitApplet *applet = GPM_INHIBIT_APPLET(_applet);
	GtkActionGroup *action_group;
	gchar *ui_path;

	static const GtkActionEntry menu_actions [] = {
		{ "About", "help-about", N_("_About"),
		  NULL, NULL,
		  G_CALLBACK (gpm_applet_dialog_about_cb) },
		{ "Help", "help-browser", N_("_Help"),
		  NULL, NULL,
		  G_CALLBACK (gpm_applet_help_cb) }
	};

	if (strcmp (iid, GPM_INHIBIT_APPLET_ID) != 0) {
		return FALSE;
	}

	action_group = gtk_action_group_new ("Inhibit Applet Actions");
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (action_group,
				      menu_actions,
				      G_N_ELEMENTS (menu_actions),
				      applet);
	ui_path = g_build_filename (INHIBIT_MENU_UI_DIR, "inhibit-applet-menu.xml", NULL);
	mate_panel_applet_setup_menu_from_file (MATE_PANEL_APPLET (applet), ui_path, action_group);
	g_free (ui_path);
	g_object_unref (action_group);

	return TRUE;
}

/**
 * this generates a main with a applet factory
 **/
MATE_PANEL_APPLET_OUT_PROCESS_FACTORY
 (/* the factory iid */
 GPM_INHIBIT_APPLET_FACTORY_ID,
 /* generates brighness applets instead of regular mate applets  */
 GPM_TYPE_INHIBIT_APPLET,
 /* the applet name */
 "InhibitApplet",
 /* our callback (with no user data) */
 gpm_applet_cb, NULL)
