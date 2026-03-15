/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005-2007 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2012-2021 MATE Developers
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

#include <glib.h>
#include <gio/gio.h>

#include "gpm-networkmanager.h"

#define NM_LISTENER_SERVICE	"org.freedesktop.NetworkManager"
#define NM_LISTENER_PATH	"/org/freedesktop/NetworkManager"
#define NM_LISTENER_INTERFACE	"org.freedesktop.NetworkManager"

/**
 * gpm_networkmanager_sleep:
 *
 * Tell NetworkManager to put the network devices to sleep
 *
 * Return value: TRUE if NetworkManager is now sleeping.
 **/
gboolean
gpm_networkmanager_sleep (void)
{
	GDBusProxy *nm_proxy = NULL;
	GVariant *result = NULL;
	GError *error = NULL;

	nm_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
						 G_DBUS_PROXY_FLAGS_NONE,
						 NULL,
						 NM_LISTENER_SERVICE,
						 NM_LISTENER_PATH,
						 NM_LISTENER_INTERFACE,
						 NULL,
						 &error);
	if (error) {
		g_warning ("%s", error->message);
		g_error_free (error);
		return FALSE;
	}
	if (!nm_proxy) {
		g_warning ("Failed to get name owner");
		return FALSE;
	}

	result = g_dbus_proxy_call_sync (nm_proxy,
					 "sleep",
					 NULL,
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 NULL,
					 &error);
	if (error) {
		g_warning ("%s", error->message);
		g_error_free (error);
		g_object_unref (G_OBJECT (nm_proxy));
		return FALSE;
	}
	if (result != NULL)
		g_variant_unref (result);

	g_object_unref (G_OBJECT (nm_proxy));
	return TRUE;
}

/**
 * gpm_networkmanager_wake:
 *
 * Tell NetworkManager to wake up all the network devices
 *
 * Return value: TRUE if NetworkManager is now awake.
 **/
gboolean
gpm_networkmanager_wake (void)
{
	GDBusProxy *nm_proxy = NULL;
	GVariant *result = NULL;
	GError *error = NULL;

	nm_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
						 G_DBUS_PROXY_FLAGS_NONE,
						 NULL,
						 NM_LISTENER_SERVICE,
						 NM_LISTENER_PATH,
						 NM_LISTENER_INTERFACE,
						 NULL,
						 &error);
	if (error) {
		g_warning ("%s", error->message);
		g_error_free (error);
		return FALSE;
	}
	if (!nm_proxy) {
		g_warning ("Failed to get name owner");
		return FALSE;
	}

	result = g_dbus_proxy_call_sync (nm_proxy,
					 "wake",
					 NULL,
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 NULL,
					 &error);
	if (error) {
		g_warning ("%s", error->message);
		g_error_free (error);
		g_object_unref (G_OBJECT (nm_proxy));
		return FALSE;
	}
	if (result != NULL)
		g_variant_unref (result);

	g_object_unref (G_OBJECT (nm_proxy));
	return TRUE;
}
