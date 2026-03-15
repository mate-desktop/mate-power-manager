/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005-2007 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2012-2021 MATE Developers
 *
 * Taken in part from:
 *  - lshal   (C) 2003 David Zeuthen, <david@fubar.dk>
 *  - notibat (C) 2004 Benjamin Kahn, <xkahn@zoned.net>
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

#include "config.h"

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <locale.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gpm-icon-names.h"
#include "gpm-common.h"
#include "gpm-manager.h"
#include "gpm-session.h"

#define GPM_DBUS_DAEMON_SERVICE "org.freedesktop.DBus"
#define GPM_DBUS_DAEMON_PATH "/org/freedesktop/DBus"

enum {
	GPM_DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER = 1,
	GPM_DBUS_REQUEST_NAME_REPLY_IN_QUEUE = 2
};

static const gchar gpm_manager_introspection_xml[] =
	"<node>"
	"  <interface name='org.mate.PowerManager'/>"
	"</node>";

static GDBusNodeInfo *gpm_manager_node_info = NULL;

/**
 * gpm_object_register:
 * @connection: What we want to register to
 * @object: The GObject we want to register
 *
 * Register org.mate.PowerManager on the session bus.
 * This function MUST be called before DBUS service will work.
 *
 * Return value: success
 **/
static gboolean
gpm_object_register (GDBusConnection *connection,
		     GObject *object)
{
	GError *error = NULL;
	GVariant *result = NULL;
	guint request_name_result;
	guint registration_id;
        static const GDBusInterfaceVTable interface_vtable = {
		NULL,
		NULL,
		NULL,
		{ 0 }
	};

	result = g_dbus_connection_call_sync (connection,
					 GPM_DBUS_DAEMON_SERVICE,
					 GPM_DBUS_DAEMON_PATH,
					 GPM_DBUS_DAEMON_SERVICE,
					 "RequestName",
					 g_variant_new ("(su)", GPM_DBUS_SERVICE, (guint) G_BUS_NAME_OWNER_FLAGS_NONE),
					 G_VARIANT_TYPE ("(u)"),
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 NULL,
					 &error);
	if (result == NULL) {
		g_debug ("ERROR: %s", error->message);
		g_error_free (error);
		/* abort as the DBUS method failed */
		g_warning ("RequestName failed!");
		return FALSE;
	}
	g_variant_get (result, "(u)", &request_name_result);
	g_variant_unref (result);

	/* already running */
	if (request_name_result != GPM_DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
		return FALSE;
	}

	if (gpm_manager_node_info == NULL) {
		gpm_manager_node_info = g_dbus_node_info_new_for_xml (gpm_manager_introspection_xml, &error);
		if (gpm_manager_node_info == NULL) {
			g_warning ("Failed to create manager introspection data: %s", error->message);
			g_error_free (error);
			return FALSE;
		}
	}

	registration_id = g_dbus_connection_register_object (connection,
						     GPM_DBUS_PATH,
						     gpm_manager_node_info->interfaces[0],
						     &interface_vtable,
						     object,
						     NULL,
						     &error);
	if (registration_id == 0) {
		g_warning ("Failed to register %s: %s", GPM_DBUS_PATH, error->message);
		g_error_free (error);
		return FALSE;
	}

	return TRUE;
}

/**
 * timed_exit_cb:
 * @loop: The main loop
 *
 * Exits the main loop, which is helpful for valgrinding g-p-m.
 *
 * Return value: FALSE, as we don't want to repeat this action.
 **/
static gboolean
timed_exit_cb (GMainLoop *loop)
{
	g_main_loop_quit (loop);
	return FALSE;
}

/**
 * gpm_main_stop_cb:
 **/
static void
gpm_main_stop_cb (GpmSession *session, GMainLoop *loop)
{
	g_main_loop_quit (loop);
}

/**
 * gpm_main_query_end_session_cb:
 **/
static void
gpm_main_query_end_session_cb (GpmSession *session, guint flags, GMainLoop *loop)
{
	/* just send response */
	gpm_session_end_session_response (session, TRUE, NULL);
}

/**
 * gpm_main_end_session_cb:
 **/
static void
gpm_main_end_session_cb (GpmSession *session, guint flags, GMainLoop *loop)
{
	/* send response */
	gpm_session_end_session_response (session, TRUE, NULL);

	/* exit loop, will unref manager */
	g_main_loop_quit (loop);
}

/**
 * main:
 **/
int
main (int argc, char *argv[])
{
	GMainLoop *loop;
	GDBusConnection *system_connection;
	GDBusConnection *session_connection;
	gboolean version = FALSE;
	gboolean timed_exit = FALSE;
	gboolean immediate_exit = FALSE;
	GpmSession *session = NULL;
	GpmManager *manager = NULL;
	GError *error = NULL;
	GVariant *result = NULL;
	GOptionContext *context;
	gint ret;
	guint timer_id;

	const GOptionEntry options[] = {
		{ "version", '\0', 0, G_OPTION_ARG_NONE, &version,
		  N_("Show version of installed program and exit"), NULL },
		{ "timed-exit", '\0', 0, G_OPTION_ARG_NONE, &timed_exit,
		  N_("Exit after a small delay (for debugging)"), NULL },
		{ "immediate-exit", '\0', 0, G_OPTION_ARG_NONE, &immediate_exit,
		  N_("Exit after the manager has loaded (for debugging)"), NULL },
		{ NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
	};

	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, MATELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	context = g_option_context_new (N_("MATE Power Manager"));
	/* TRANSLATORS: program name, a simple app to view pending updates */
	g_option_context_add_main_entries (context, options, GETTEXT_PACKAGE);
	g_option_context_set_translation_domain(context, GETTEXT_PACKAGE);
	g_option_context_set_summary (context, _("MATE Power Manager"));
	g_option_context_parse (context, &argc, &argv, NULL);

	if (version) {
		g_print ("Version %s\n", VERSION);
		goto unref_program;
	}

	gtk_init (&argc, &argv);

	g_debug ("MATE %s %s", GPM_NAME, VERSION);

	/* check dbus connections, exit if not valid */
	system_connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
	if (error) {
		g_warning ("%s", error->message);
		g_error_free (error);
		g_error ("This program cannot start until you start "
		         "the dbus system service.\n"
		         "It is <b>strongly recommended</b> you reboot "
		         "your computer after starting this service.");
	}

	error = NULL;
	session_connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
	if (error) {
		g_warning ("%s", error->message);
		g_error_free (error);
		g_error ("This program cannot start until you start the "
		         "dbus session service.\n\n"
		         "This is usually started automatically in X "
		         "or mate startup when you start a new session.");
	}

	/* add application specific icons to search path */
	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
                                           GPM_ICONS_DATA);

	loop = g_main_loop_new (NULL, FALSE);

	/* optionally register with the session */
	session = gpm_session_new ();
	g_signal_connect (session, "stop", G_CALLBACK (gpm_main_stop_cb), loop);
	g_signal_connect (session, "query-end-session", G_CALLBACK (gpm_main_query_end_session_cb), loop);
	g_signal_connect (session, "end-session", G_CALLBACK (gpm_main_end_session_cb), loop);
	gpm_session_register_client (session, "mate-power-manager", getenv ("DESKTOP_AUTOSTART_ID"));

	/* create a new gui object */
	manager = gpm_manager_new ();

	if (!gpm_object_register (session_connection, G_OBJECT (manager))) {
		g_error ("%s is already running in this session.", GPM_NAME);
		goto unref_program;
	}

	error = NULL;
	if (!gpm_manager_register_dbus (manager, session_connection, &error)) {
		g_error ("Failed to export D-Bus objects: %s", error->message);
		g_error_free (error);
		goto unref_program;
	}

	/* register to be a policy agent, just like kpackagekit does */
	result = g_dbus_connection_call_sync (system_connection,
					 GPM_DBUS_DAEMON_SERVICE,
					 GPM_DBUS_DAEMON_PATH,
					 GPM_DBUS_DAEMON_SERVICE,
					 "RequestName",
					 g_variant_new ("(su)",
							"org.freedesktop.Policy.Power",
							(guint) G_BUS_NAME_OWNER_FLAGS_REPLACE),
					 G_VARIANT_TYPE ("(u)"),
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 NULL,
					 NULL);
	ret = 0;
	if (result != NULL) {
		g_variant_get (result, "(u)", &ret);
		g_variant_unref (result);
	}
	switch (ret) {
	case GPM_DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER:
		g_debug ("Successfully acquired interface org.freedesktop.Policy.Power.");
		break;
	case GPM_DBUS_REQUEST_NAME_REPLY_IN_QUEUE:
		g_debug ("Queued for interface org.freedesktop.Policy.Power.");
		break;
	default:
		break;
	};

	/* Only timeout and close the mainloop if we have specified it
	 * on the command line */
	if (timed_exit) {
		timer_id = g_timeout_add_seconds (20, (GSourceFunc) timed_exit_cb, loop);
		g_source_set_name_by_id (timer_id, "[GpmMain] timed-exit");
	}

	if (immediate_exit == FALSE) {
		g_main_loop_run (loop);
	}

	g_main_loop_unref (loop);

	g_object_unref (session);
	g_object_unref (manager);
unref_program:
	g_option_context_free (context);
	return 0;
}
