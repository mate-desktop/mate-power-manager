/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 Jaap Haitsma <jaap@haitsma.org>
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2005-2008 Richard Hughes <richard@hughsie.com>
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
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

/* local .la */
#if !GTK_CHECK_VERSION (3, 0, 0)
#include <egg-unique.h>
#endif

#include "gpm-common.h"
#include "egg-debug.h"
#include "gpm-prefs-core.h"

/**
 * gpm_prefs_help_cb
 * @prefs: This prefs class instance
 *
 * What to do when help is requested
 **/
static void
gpm_prefs_help_cb (GpmPrefs *prefs)
{
	gpm_help_display ("preferences");
}

/**
 * gpm_prefs_activated_cb
 * @prefs: This prefs class instance
 *
 * We have been asked to show the window
 **/
static void
#if GTK_CHECK_VERSION (3, 0, 0)
gpm_prefs_activated_cb (GtkApplication *app, GpmPrefs *prefs)
{
	gpm_prefs_activate_window (app, prefs);
}
#else
gpm_prefs_activated_cb (EggUnique *egg_unique, GpmPrefs *prefs)
{
	gpm_prefs_activate_window (prefs);
}
#endif

/**
 * main:
 **/
int
main (int argc, char **argv)
{
	gboolean verbose = FALSE;
	GOptionContext *context;
	GpmPrefs *prefs = NULL;
	gboolean ret;
#if GTK_CHECK_VERSION (3, 0, 0)
	GtkApplication *app;
	GtkWidget *window;
	gint status;
#else
	EggUnique *egg_unique;
#endif

	const GOptionEntry options[] = {
		{ "verbose", '\0', 0, G_OPTION_ARG_NONE, &verbose,
		  N_("Show extra debugging information"), NULL },
		{ NULL}
	};

	context = g_option_context_new (N_("MATE Power Preferences"));

	bindtextdomain (GETTEXT_PACKAGE, MATELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_option_context_set_translation_domain(context, GETTEXT_PACKAGE);
	g_option_context_add_main_entries (context, options, GETTEXT_PACKAGE);
	g_option_context_add_group (context, gtk_get_option_group (FALSE));
	g_option_context_parse (context, &argc, &argv, NULL);

#if !GTK_CHECK_VERSION (3, 0, 0)
	gtk_init (&argc, &argv);
#endif
	egg_debug_init (verbose);

#if GTK_CHECK_VERSION (3, 0, 0)
	gdk_init (&argc, &argv);
	app = gtk_application_new("org.mate.PowerManager.Preferences", 0);
#else
	/* are we already activated? */
	egg_unique = egg_unique_new ();
	ret = egg_unique_assign (egg_unique, "org.mate.PowerManager.Preferences");
	if (!ret) {
		goto unique_out;
	}
#endif

	prefs = gpm_prefs_new ();

#if GTK_CHECK_VERSION (3, 0, 0)
	window = gpm_window (prefs);
	g_signal_connect (app, "activate",
#else
	g_signal_connect (egg_unique, "activated",
#endif
			  G_CALLBACK (gpm_prefs_activated_cb), prefs);
	g_signal_connect (prefs, "action-help",
			  G_CALLBACK (gpm_prefs_help_cb), prefs);
#if GTK_CHECK_VERSION (3, 0, 0)
	g_signal_connect_swapped (prefs, "action-close",
			  G_CALLBACK (gtk_widget_destroy), window);
#else
	g_signal_connect_swapped (prefs, "action-close",
			  G_CALLBACK (gtk_main_quit), NULL);
#endif

#if GTK_CHECK_VERSION (3, 0, 0)
	status = g_application_run (G_APPLICATION (app), argc, argv);
#else
	gtk_main ();
#endif
	g_object_unref (prefs);

#if GTK_CHECK_VERSION (3, 0, 0)
	g_object_unref (app);
#else
unique_out:
	g_object_unref (egg_unique);
#endif

/* seems to not work...
	g_option_context_free (context); */

#if GTK_CHECK_VERSION (3, 0, 0)
	return status;
#else
	return 0;
#endif
}
