/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008-2010 Richard Hughes <richard@hughsie.com>
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

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

#ifdef HAVE_X11
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <gdk/gdkx.h>
#endif /* HAVE_X11 */

#ifdef HAVE_WAYLAND
#include <gdk/gdkwayland.h>
#endif /* HAVE_WAYLAND */

#include <gtk/gtk.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include "egg-discrete.h"

#ifdef HAVE_DDCUTIL
#include <ddcutil_status_codes.h>
#include <ddcutil_c_api.h>

/* VCP 0x10 is "Brightness"; the nVidia driver fails the read-back of a write
 * that actually landed, hence the "2" variant below and the retries. */
#define GPM_DDC_BRIGHTNESS_CODE	0x10
#define GPM_DDC_RETRIES		3
#define GPM_DDC_OPEN_RETRIES	5
#define GPM_DDC_APPLY_MS	120
#endif

#include "gpm-brightness.h"
#include "gpm-common.h"
#include "gpm-marshal.h"

#define GPM_SOLE_SETTER_USE_CACHE	TRUE	/* this may be insanity */

/* A gamma ramp of 0 makes the screen unreadable - there is no hardware
 * backlight to restore it with a button, so keep a floor. */
#define GPM_GAMMA_MIN_LEVEL		0.10

struct GpmBrightnessPrivate
{
	gboolean		 has_changed_events;
	gboolean		 cache_trusted;
	guint			 cache_percentage;
	guint			 last_set_hw;
	gboolean		 has_extension;
	gboolean		 use_gamma;	/* no "Backlight" output property: drive CRTC gamma */
	gdouble			 gamma_level;	/* 0.0 .. 1.0, 1.0 = untouched */
#ifdef HAVE_DDCUTIL
	gboolean		 use_ddc;	/* real monitor brightness through DDC/CI */
	DDCA_Display_Handle	 ddc_handle;
	guint			 ddc_max;	/* VCP 0x10 maximum, read once */
	guint			 ddc_apply_idle;	/* debounce for interactive changes */
	guint			 ddc_pending;	/* percentage waiting to be written */
#endif
	gboolean		 hw_changed;
	gint			 extension_levels;
	gint			 extension_current;
#ifdef HAVE_X11
	Atom			 backlight;
	Display			*dpy;
	GdkWindow		*root_window;
	guint			 shared_value;
	/* A cache of XRRScreenResources is used as XRRGetScreenResources is expensive */
	GPtrArray		*resources;
#endif
};

enum {
	BRIGHTNESS_CHANGED,
	LAST_SIGNAL
};

#ifdef HAVE_X11
typedef enum {
	ACTION_BACKLIGHT_GET,
	ACTION_BACKLIGHT_SET,
	ACTION_BACKLIGHT_INC,
	ACTION_BACKLIGHT_DEC
} GpmXRandROp;
#endif

G_DEFINE_TYPE_WITH_PRIVATE (GpmBrightness, gpm_brightness, G_TYPE_OBJECT)

static guint signals [LAST_SIGNAL] = { 0 };
static gpointer gpm_brightness_object = NULL;

/**
 * gpm_brightness_helper_strtoint:
 * @text: The text to be converted
 * @value: The return numeric return value
 *
 * Convert a string to a signed integer value in a safe way.
 *
 * Return value: %TRUE if the string was converted correctly
 **/
static gboolean
gpm_brightness_helper_strtoint (const gchar *text, gint *value)
{
	gchar *endptr = NULL;
	gint64 value_raw;

	if (text == NULL)
		return FALSE;

	value_raw = g_ascii_strtoll (text, &endptr, 10);

	if (endptr == text)
		return FALSE;
	if (errno == ERANGE || value_raw > G_MAXINT || value_raw < G_MININT)
		return FALSE;

	*value = (gint) value_raw;
	return TRUE;
}

/**
 * gpm_brightness_helper_get_value:
 **/
static gint
gpm_brightness_helper_get_value (const gchar *argument)
{
	gboolean ret;
	GError *error = NULL;
	gchar *stdout_data = NULL;
	gint exit_status = 0;
	gint value = -1;
	gchar *command = NULL;

	/* get the data */
	command = g_strdup_printf (LIBEXECDIR "/mate-power-backlight-helper --%s", argument);
	ret = g_spawn_command_line_sync (command,
					 &stdout_data, NULL, &exit_status, &error);
	if (!ret) {
		g_error ("failed to get value: %s", error->message);
		g_error_free (error);
		goto out;
	}
	g_debug ("executing %s retval: %i", command, exit_status);

	/* parse for a number */
	ret = gpm_brightness_helper_strtoint (stdout_data, &value);
	if (!ret)
		goto out;
out:
	g_free (command);
	g_free (stdout_data);
	return value;
}

/**
 * gpm_brightness_helper_set_value:
 **/
static gboolean
gpm_brightness_helper_set_value (const gchar *argument, gint value)
{
	gboolean ret;
	GError *error = NULL;
	gint exit_status = 0;
	gchar *command = NULL;

	/* get the data */
	command = g_strdup_printf ("pkexec " LIBEXECDIR "/mate-power-backlight-helper --%s %i", argument, value);
	ret = g_spawn_command_line_sync (command, NULL, NULL, &exit_status, &error);
	if (!ret) {
		g_error ("failed to get value: %s", error->message);
		g_error_free (error);
		goto out;
	}
	g_debug ("executing %s retval: %i", command, exit_status);
out:
	g_free (command);
	return ret;
}

/**
 * gpm_brightness_get_step:
 * @levels: The number of levels supported
 * Return value: the amount of hardware steps to do on each increment or decrement
 **/
static guint
gpm_brightness_get_step (guint levels)
{
	/* macbook pro has a bazzillion brightness levels, do in 5% steps */
	if (levels > 20)
		return levels / 20;
	return 1;
}

#ifdef HAVE_X11

/**
 * gpm_brightness_output_get_internal:
 **/
static gboolean
gpm_brightness_output_get_internal (GpmBrightness *brightness, RROutput output, guint *cur)
{
	unsigned long nitems;
	unsigned long bytes_after;
	guint *prop;
	Atom actual_type;
	int actual_format;
	gboolean ret = FALSE;

	g_return_val_if_fail (GPM_IS_BRIGHTNESS (brightness), FALSE);

	if (brightness->priv->backlight == None)
		return FALSE;

	if (XRRGetOutputProperty (brightness->priv->dpy, output, brightness->priv->backlight,
				  0, 4, False, False, None,
				  &actual_type, &actual_format,
				  &nitems, &bytes_after, ((unsigned char **)&prop)) != Success) {
		g_debug ("failed to get property");
		return FALSE;
	}
	if (actual_type == XA_INTEGER && nitems == 1 && actual_format == 32) {
		memcpy (cur, prop, sizeof (guint));
		ret = TRUE;
	}
	XFree (prop);
	return ret;
}

/**
 * gpm_brightness_output_set_internal:
 **/
static gboolean
gpm_brightness_output_set_internal (GpmBrightness *brightness, RROutput output, guint value)
{
	GdkDisplay *display;

	gboolean ret = TRUE;

	g_return_val_if_fail (GPM_IS_BRIGHTNESS (brightness), FALSE);

	/* don't abort on error */
	display = gdk_display_get_default ();
	gdk_x11_display_error_trap_push (display);
	XRRChangeOutputProperty (brightness->priv->dpy, output, brightness->priv->backlight, XA_INTEGER, 32,
				 PropModeReplace, (unsigned char *) &value, 1);
	XFlush (brightness->priv->dpy);
	gdk_display_flush (display);
	if (gdk_x11_display_error_trap_pop (display)) {
		g_warning ("failed to XRRChangeOutputProperty for brightness %u", value);
		ret = FALSE;
	}
	/* we changed the hardware */
	if (ret)
		brightness->priv->hw_changed = TRUE;
	return ret;
}

#ifdef HAVE_DDCUTIL
static gboolean gpm_brightness_ddc_open (GpmBrightness *brightness);
static gboolean gpm_brightness_ddc_set (GpmBrightness *brightness, guint percentage);
#endif
#ifdef HAVE_X11
static gboolean gpm_brightness_gamma_apply (GpmBrightness *brightness, gdouble level);
#endif

/**
 * gpm_brightness_setup_display:
 **/
static gboolean
gpm_brightness_setup_display (GpmBrightness *brightness)
{
	gint major, minor;

	g_return_val_if_fail (GPM_IS_BRIGHTNESS (brightness), FALSE);

	/* get the display */
	brightness->priv->dpy = GDK_DISPLAY_XDISPLAY (gdk_display_get_default());
	if (!brightness->priv->dpy) {
		g_error ("Cannot open display");
		return FALSE;
	}
	/* is XRandR new enough? */
	if (!XRRQueryVersion (brightness->priv->dpy, &major, &minor)) {
		g_debug ("RandR extension missing");
		return FALSE;
	}
	if (major < 1 || (major == 1 && minor < 3)) {
		g_debug ("RandR version %d.%d too old", major, minor);
		return FALSE;
	}
	/* Can we support "Backlight" */
	brightness->priv->backlight = XInternAtom (brightness->priv->dpy, "Backlight", True);
	if (brightness->priv->backlight == None) {
		/* Do we support "BACKLIGHT" (legacy) */
		brightness->priv->backlight = XInternAtom (brightness->priv->dpy, "BACKLIGHT", True);
		if (brightness->priv->backlight == None) {
			/* Desktop monitors have no backlight property at all: the only
			 * thing we can drive is the CRTC gamma ramp, which is what
			 * "xrandr --brightness" does.  Keep the backend alive for that
			 * instead of reporting "no hardware". */
			g_debug ("No outputs have backlight property; using CRTC gamma");
			brightness->priv->use_gamma = TRUE;
			brightness->priv->gamma_level = 1.0;
			return TRUE;
		}
	}

	/* The atom can exist in the server without any output carrying it - a
	 * desktop monitor has no backlight property at all - so ask the outputs. */
	{
		XRRScreenResources *resources;
		gboolean any_output = FALSE;

		gdk_x11_display_error_trap_push (gdk_display_get_default ());
		gdk_x11_display_error_trap_push (gdk_display_get_default ());
		resources = XRRGetScreenResourcesCurrent (brightness->priv->dpy,
							  DefaultRootWindow (brightness->priv->dpy));
		if (resources != NULL) {
			gint i;

			for (i = 0; i < resources->noutput; i++) {
				XRRPropertyInfo *info;

				info = XRRQueryOutputProperty (brightness->priv->dpy,
							       resources->outputs[i],
							       brightness->priv->backlight);
				if (info != NULL) {
					any_output = TRUE;
					XFree (info);
					break;
				}
			}
			XRRFreeScreenResources (resources);
		}
		gdk_x11_display_error_trap_pop_ignored (gdk_display_get_default ());

		if (!any_output) {
#ifdef HAVE_DDCUTIL
			/* Desktop monitor: DDC/CI is the only way to touch the real
			 * backlight.  The gamma ramp below stays as the fallback. */
			if (gpm_brightness_ddc_open (brightness)) {
				g_debug ("No backlight property; using DDC/CI");
				brightness->priv->use_ddc = TRUE;
				/* A previous run may have left the screen dimmed through the
				 * gamma ramp; DDC/CI drives the backlight, so hand the picture
				 * back at full range. */
				brightness->priv->use_gamma = FALSE;
				gpm_brightness_gamma_apply (brightness, 1.0);
				return TRUE;
			}
			g_debug ("DDC/CI unavailable");
#endif
			g_debug ("No output carries a backlight property; using CRTC gamma");
			brightness->priv->use_gamma = TRUE;
			brightness->priv->gamma_level = 1.0;
			return TRUE;
		}
	}

	return TRUE;
}

#ifdef HAVE_DDCUTIL
static gboolean
gpm_brightness_ddc_open (GpmBrightness *brightness)
{
	DDCA_Display_Info_List *list;
	gboolean opened = FALSE;

	/* The bus answers through the nVidia driver intermittently: a single failed
	 * open is not evidence that the monitor has no DDC/CI. */
	for (gint attempt = 0; attempt < GPM_DDC_OPEN_RETRIES && !opened; attempt++) {
		list = NULL;
		if (ddca_get_display_info_list2 (FALSE, &list) != DDCRC_OK || list == NULL) {
			g_usleep (G_USEC_PER_SEC / 3);
			continue;
		}

		for (guint i = 0; i < list->ct && !opened; i++) {
			DDCA_Status rc;

			rc = ddca_open_display2 (list->info[i].dref, TRUE,
						 &brightness->priv->ddc_handle);
			g_debug ("DDC open attempt %d display %u: rc=%d (%s)", attempt, i,
				 rc, ddca_rc_name (rc));
			if (rc == DDCRC_OK && brightness->priv->ddc_handle != NULL)
				opened = TRUE;
		}

		ddca_free_display_info_list (list);

		if (!opened)
			g_usleep (G_USEC_PER_SEC / 3);
	}

	if (opened) {
		DDCA_Non_Table_Vcp_Value val;

		/* Cache the maximum: reading it on every write costs another round trip
		 * that this driver fails often enough to make the write look broken. */
		brightness->priv->ddc_max = 100;
		for (gint i = 0; i < GPM_DDC_OPEN_RETRIES; i++) {
			if (ddca_get_non_table_vcp_value (brightness->priv->ddc_handle,
							  GPM_DDC_BRIGHTNESS_CODE,
							  &val) == DDCRC_OK) {
				guint max = ((guint) val.sh << 8) | val.sl;
				guint cur = ((guint) val.mh << 8) | val.ml;

				/* A corrupted reply looks like a plausible small maximum
				 * (measured: 33 for a monitor whose maximum is 100), and the
				 * current value then exceeds it.  Take the largest sane reading
				 * over several attempts instead of trusting the first. */
				if (max > 0 && max <= 1000 && cur <= max &&
				    max > brightness->priv->ddc_max)
					brightness->priv->ddc_max = max;
			}
			g_usleep (G_USEC_PER_SEC / 5);
		}
		g_debug ("DDC maximum brightness: %u", brightness->priv->ddc_max);
	}

	return opened;
}

static gboolean
gpm_brightness_ddc_get (GpmBrightness *brightness, guint *percentage)
{
	DDCA_Non_Table_Vcp_Value val;
	guint max;
	guint cur;
	gint i;

	for (i = 0; i < GPM_DDC_RETRIES; i++) {
		if (ddca_get_non_table_vcp_value (brightness->priv->ddc_handle,
						  GPM_DDC_BRIGHTNESS_CODE,
						  &val) != DDCRC_OK)
			continue;

		max = ((guint) val.sh << 8) | val.sl;
		cur = ((guint) val.mh << 8) | val.ml;
		if (max == 0)
			max = brightness->priv->ddc_max;

		*percentage = (cur * 100 + max / 2) / max;
		return TRUE;
	}

	return FALSE;
}

/* A DDC/CI write costs hundreds of milliseconds, while dragging a slider emits a
 * stream of changes: writing each one queues up on the bus and some writes are
 * lost.  Only the last value matters, so coalesce them. */
static gboolean
gpm_brightness_ddc_apply_cb (gpointer data)
{
	GpmBrightness *brightness = data;

	brightness->priv->ddc_apply_idle = 0;

	if (brightness->priv->use_ddc && brightness->priv->ddc_handle != NULL)
		gpm_brightness_ddc_set (brightness, brightness->priv->ddc_pending);

	return G_SOURCE_REMOVE;
}

static gboolean
gpm_brightness_ddc_set (GpmBrightness *brightness, guint percentage)
{
	DDCA_Non_Table_Vcp_Value val;
	guint max;
	guint target;
	gint i;

	(void) val;
	max = brightness->priv->ddc_max;
	if (max == 0)
		return FALSE;

	target = (percentage * max + 50) / 100;

	for (i = 0; i < GPM_DDC_RETRIES; i++) {
		DDCA_Status rc;

		/* Never verify: this is what "--noverify" does on the command line and
		 * the only mode that reports success on the nVidia driver. */
		rc = ddca_set_non_table_vcp_value2 (brightness->priv->ddc_handle,
						    GPM_DDC_BRIGHTNESS_CODE,
						    (target >> 8) & 0xff,
						    target & 0xff);
		g_debug ("DDC set %u%% -> raw %u: rc=%d (%s)", percentage, target, rc,
			 ddca_rc_name (rc));
		if (rc == DDCRC_OK)
			return TRUE;

		/* A handle that saw an error tends to keep failing; reopen it. */
		ddca_close_display (brightness->priv->ddc_handle);
		brightness->priv->ddc_handle = NULL;
		if (!gpm_brightness_ddc_open (brightness))
			return FALSE;
	}

	return FALSE;
}
#endif /* HAVE_DDCUTIL */

/**
 * gpm_brightness_gamma_apply:
 * @brightness: the brightness object
 * @level: 0.0 .. 1.0, where 1.0 leaves the output untouched
 *
 * Scales every active CRTC's gamma ramp, i.e. exactly what
 * "xrandr --output <out> --brightness <level>" does.  This is the only way to
 * dim a desktop monitor whose output carries no "Backlight" property.
 *
 * Return value: %TRUE if at least one CRTC was programmed
 **/
static gboolean
gpm_brightness_gamma_apply (GpmBrightness *brightness, gdouble level)
{
	XRRScreenResources *resources;
	GdkDisplay *display;
	gboolean applied = FALSE;
	gint i;

	g_return_val_if_fail (GPM_IS_BRIGHTNESS (brightness), FALSE);

	level = CLAMP (level, GPM_GAMMA_MIN_LEVEL, 1.0);

	/* A CRTC that cannot take a ramp answers with an X error, and an untrapped
	 * X error kills the daemon; skip such CRTCs instead of dying. */
	display = gdk_display_get_default ();
	gdk_x11_display_error_trap_push (display);

	resources = XRRGetScreenResourcesCurrent (brightness->priv->dpy,
						  GDK_WINDOW_XID (brightness->priv->root_window));
	if (resources == NULL) {
		gdk_x11_display_error_trap_pop_ignored (display);
		return FALSE;
	}

	for (i = 0; i < resources->ncrtc; i++) {
		XRRCrtcInfo *crtc_info;
		XRRCrtcGamma *gamma;
		gint size;
		gint j;

		crtc_info = XRRGetCrtcInfo (brightness->priv->dpy, resources, resources->crtcs[i]);
		if (crtc_info == NULL)
			continue;
		if (crtc_info->mode == None) {
			/* disabled CRTC: nothing on screen to dim */
			XRRFreeCrtcInfo (crtc_info);
			continue;
		}

		size = XRRGetCrtcGammaSize (brightness->priv->dpy, resources->crtcs[i]);
		if (size <= 0) {
			XRRFreeCrtcInfo (crtc_info);
			continue;
		}

		gamma = XRRAllocGamma (size);
		for (j = 0; j < size; j++) {
			gdouble ramp = (size > 1) ? (gdouble) j / (size - 1) : 0.0;
			guint16 value = (guint16) (ramp * level * 65535.0 + 0.5);

			gamma->red[j] = value;
			gamma->green[j] = value;
			gamma->blue[j] = value;
		}

		XRRSetCrtcGamma (brightness->priv->dpy, resources->crtcs[i], gamma);
		XRRFreeGamma (gamma);
		XRRFreeCrtcInfo (crtc_info);
		applied = TRUE;
	}

	XRRFreeScreenResources (resources);
	XSync (brightness->priv->dpy, False);

	if (gdk_x11_display_error_trap_pop (display))
		g_debug ("X error while programming the gamma ramp");

	return applied;
}

/**
 * gpm_brightness_output_get_limits:
 **/
static gboolean
gpm_brightness_output_get_limits (GpmBrightness *brightness, RROutput output,
					 guint *min, guint *max)
{
	XRRPropertyInfo *info;
	gboolean ret = TRUE;

	g_return_val_if_fail (GPM_IS_BRIGHTNESS (brightness), FALSE);

	info = XRRQueryOutputProperty (brightness->priv->dpy, output, brightness->priv->backlight);
	if (info == NULL) {
		g_debug ("could not get output property");
		return FALSE;
	}
	if (!info->range || info->num_values != 2) {
		g_debug ("was not range");
		ret = FALSE;
		goto out;
	}
	*min = info->values[0];
	*max = info->values[1];
out:
	XFree (info);
	return ret;
}

/**
 * gpm_brightness_output_get_percentage:
 **/
static gboolean
gpm_brightness_output_get_percentage (GpmBrightness *brightness, RROutput output)
{
	guint cur;
	gboolean ret;
	guint min, max;
	guint percentage;

	g_return_val_if_fail (GPM_IS_BRIGHTNESS (brightness), FALSE);

	ret = gpm_brightness_output_get_internal (brightness, output, &cur);
	if (!ret)
		return FALSE;
	ret = gpm_brightness_output_get_limits (brightness, output, &min, &max);
	if (!ret || min == max)
		return FALSE;
	g_debug ("hard value=%u, min=%u, max=%u", cur, min, max);
	percentage = egg_discrete_to_percent (cur, (max-min)+1);
	g_debug ("percentage %u", percentage);
	brightness->priv->shared_value = percentage;
	return TRUE;
}

/**
 * gpm_brightness_output_down:
 **/
static gboolean
gpm_brightness_output_down (GpmBrightness *brightness, RROutput output)
{
	guint cur;
	guint step;
	gboolean ret;
	guint min, max;

	g_return_val_if_fail (GPM_IS_BRIGHTNESS (brightness), FALSE);

	ret = gpm_brightness_output_get_internal (brightness, output, &cur);
	if (!ret)
		return FALSE;
	ret = gpm_brightness_output_get_limits (brightness, output, &min, &max);
	if (!ret || min == max)
		return FALSE;
	g_debug ("hard value=%u, min=%u, max=%u", cur, min, max);
	if (cur == min) {
		g_debug ("already min");
		return TRUE;
	}
	step = gpm_brightness_get_step ((max-min)+1);
	if (cur < step) {
		g_debug ("truncating to %u", min);
		cur = min;
	} else {
		cur -= step;
	}
	ret = gpm_brightness_output_set_internal (brightness, output, cur);
	return ret;
}

/**
 * gpm_brightness_output_up:
 **/
static gboolean
gpm_brightness_output_up (GpmBrightness *brightness, RROutput output)
{
	guint cur;
	gboolean ret;
	guint min, max;

	g_return_val_if_fail (GPM_IS_BRIGHTNESS (brightness), FALSE);

	ret = gpm_brightness_output_get_internal (brightness, output, &cur);
	if (!ret)
		return FALSE;
	ret = gpm_brightness_output_get_limits (brightness, output, &min, &max);
	if (!ret || min == max)
		return FALSE;
	g_debug ("hard value=%u, min=%u, max=%u", cur, min, max);
	if (cur == max) {
		g_debug ("already max");
		return TRUE;
	}
	cur += gpm_brightness_get_step ((max-min)+1);
	if (cur > max) {
		g_debug ("truncating to %u", max);
		cur = max;
	}
	ret = gpm_brightness_output_set_internal (brightness, output, cur);
	return ret;
}

/**
 * gpm_brightness_output_set:
 **/
static gboolean
gpm_brightness_output_set (GpmBrightness *brightness, RROutput output)
{
	guint cur;
	gboolean ret;
	guint min, max;
	gint i;
	gint shared_value_abs;
	guint step;

	g_return_val_if_fail (GPM_IS_BRIGHTNESS (brightness), FALSE);

	ret = gpm_brightness_output_get_internal (brightness, output, &cur);
	if (!ret)
		return FALSE;
	ret = gpm_brightness_output_get_limits (brightness, output, &min, &max);
	if (!ret || min == max)
		return FALSE;

	shared_value_abs = egg_discrete_from_percent (brightness->priv->shared_value, (max-min)+1);
	g_debug ("percent=%u, absolute=%i", brightness->priv->shared_value, shared_value_abs);

	g_debug ("hard value=%u, min=%u, max=%u", cur, min, max);
	if (shared_value_abs > (gint) max)
		shared_value_abs = max;
	if (shared_value_abs < (gint) min)
		shared_value_abs = min;
	if ((gint) cur == shared_value_abs) {
		g_debug ("already set %u", cur);
		return TRUE;
	}

	/* step the correct way */
	if ((gint) cur < shared_value_abs) {
		/* some adaptors have a large number of steps */
		step = gpm_brightness_get_step (shared_value_abs - cur);
		g_debug ("using step of %u", step);

		/* going up */
		for (i=cur; i<=shared_value_abs; i+=step) {
			ret = gpm_brightness_output_set_internal (brightness, output, i);
			if (!ret)
				break;
			if ((gint) cur != shared_value_abs)
				g_usleep (1000 * GPM_BRIGHTNESS_DIM_INTERVAL);
		}
	} else {
		/* some adaptors have a large number of steps */
		step = gpm_brightness_get_step (cur - shared_value_abs);
		g_debug ("using step of %u", step);

		/* going down */
		for (i=cur; i>=shared_value_abs; i-=step) {
			ret = gpm_brightness_output_set_internal (brightness, output, i);
			if (!ret)
				break;
			if ((gint) cur != shared_value_abs)
				g_usleep (1000 * GPM_BRIGHTNESS_DIM_INTERVAL);
		}
	}
	return TRUE;
}

/**
 * gpm_brightness_foreach_resource:
 **/
static gboolean
gpm_brightness_foreach_resource (GpmBrightness *brightness, GpmXRandROp op, XRRScreenResources *resources)
{
	gint i;
	gboolean ret;
	gboolean success_any = FALSE;
	RROutput output;

	g_return_val_if_fail (GPM_IS_BRIGHTNESS (brightness), FALSE);

	/* do for each output */
	for (i=0; i<resources->noutput; i++) {
		output = resources->outputs[i];
		g_debug ("resource %i of %i", i+1, resources->noutput);
		if (op==ACTION_BACKLIGHT_GET) {
			ret = gpm_brightness_output_get_percentage (brightness, output);
		} else if (op==ACTION_BACKLIGHT_INC) {
			ret = gpm_brightness_output_up (brightness, output);
		} else if (op==ACTION_BACKLIGHT_DEC) {
			ret = gpm_brightness_output_down (brightness, output);
		} else if (op==ACTION_BACKLIGHT_SET) {
			ret = gpm_brightness_output_set (brightness, output);
		} else {
			ret = FALSE;
			g_warning ("op not known");
		}
		if (ret) {
			success_any = TRUE;
		}
	}
	return success_any;
}

/**
 * gpm_brightness_foreach_screen:
 **/
static gboolean
gpm_brightness_foreach_screen (GpmBrightness *brightness, GpmXRandROp op)
{
	guint i;
	guint length;
	XRRScreenResources *resource;
	gboolean ret;
	gboolean success_any = FALSE;

	g_return_val_if_fail (GPM_IS_BRIGHTNESS (brightness), FALSE);

	/* Return immediately if we can't use XRandR */
	if (!brightness->priv->has_extension)
		return FALSE;

#ifdef HAVE_DDCUTIL
	/* Real monitor brightness first; drop to the gamma ramp if the monitor
	 * stops answering mid-session. */
	if (brightness->priv->use_ddc) {
		gboolean ok = FALSE;

		switch (op) {
		case ACTION_BACKLIGHT_GET:
			/* A read failing is normal on this driver - keep the cached value
			 * and keep DDC/CI, otherwise the next call ends up in the sysfs
			 * helper, which has nothing to write to on a desktop. */
			if (gpm_brightness_ddc_get (brightness, &brightness->priv->shared_value))
				return TRUE;
			brightness->priv->shared_value = brightness->priv->cache_percentage;
			return TRUE;
		case ACTION_BACKLIGHT_SET:
			brightness->priv->ddc_pending = brightness->priv->shared_value;
			if (brightness->priv->ddc_apply_idle != 0)
				g_source_remove (brightness->priv->ddc_apply_idle);
			brightness->priv->ddc_apply_idle =
				g_timeout_add (GPM_DDC_APPLY_MS, gpm_brightness_ddc_apply_cb, brightness);
			ok = TRUE;
			break;
		case ACTION_BACKLIGHT_INC:
		case ACTION_BACKLIGHT_DEC:
			{
				guint current;

				if (gpm_brightness_ddc_get (brightness, &current)) {
					gint next = (op == ACTION_BACKLIGHT_INC)
						? MIN ((gint) current + 10, 100)
						: MAX ((gint) current - 10, 0);

					brightness->priv->shared_value = (guint) next;
					brightness->priv->ddc_pending = (guint) next;
					if (brightness->priv->ddc_apply_idle != 0)
						g_source_remove (brightness->priv->ddc_apply_idle);
					brightness->priv->ddc_apply_idle =
						g_timeout_add (GPM_DDC_APPLY_MS,
							       gpm_brightness_ddc_apply_cb, brightness);
					ok = TRUE;
				}
			}
			break;
		default:
			break;
		}

		if (ok) {
			brightness->priv->hw_changed = TRUE;
			return TRUE;
		}

		g_debug ("DDC/CI failed; falling back to the gamma ramp");
		brightness->priv->use_ddc = FALSE;
		/* Arm the fallback here: without this the call returned FALSE and the
		 * sysfs helper took over, which does nothing on a desktop. */
		brightness->priv->use_gamma = TRUE;
		brightness->priv->gamma_level =
			CLAMP (brightness->priv->shared_value / 100.0,
			       GPM_GAMMA_MIN_LEVEL, 1.0);
	}
#endif

	/* Desktop monitor: no per-output property to write, so the gamma ramp is
	 * the hardware.  The value is kept here because a gamma ramp has no
	 * "current brightness" to read back. */
	if (brightness->priv->use_gamma) {
		gdouble step = 10.0 / 100.0;

		switch (op) {
		case ACTION_BACKLIGHT_GET:
			brightness->priv->shared_value =
				(guint) (brightness->priv->gamma_level * 100.0 + 0.5);
			return TRUE;
		case ACTION_BACKLIGHT_SET:
			brightness->priv->gamma_level =
				CLAMP (brightness->priv->shared_value / 100.0,
				       GPM_GAMMA_MIN_LEVEL, 1.0);
			brightness->priv->shared_value =
				MAX (brightness->priv->shared_value,
				     (guint) (GPM_GAMMA_MIN_LEVEL * 100.0 + 0.5));
			break;
		case ACTION_BACKLIGHT_INC:
			brightness->priv->gamma_level =
				CLAMP (brightness->priv->gamma_level + step,
				       GPM_GAMMA_MIN_LEVEL, 1.0);
			brightness->priv->shared_value =
				(guint) (brightness->priv->gamma_level * 100.0 + 0.5);
			break;
		case ACTION_BACKLIGHT_DEC:
			brightness->priv->gamma_level =
				CLAMP (brightness->priv->gamma_level - step,
				       GPM_GAMMA_MIN_LEVEL, 1.0);
			brightness->priv->shared_value =
				(guint) (brightness->priv->gamma_level * 100.0 + 0.5);
			break;
		default:
			return FALSE;
		}

		if (gpm_brightness_gamma_apply (brightness, brightness->priv->gamma_level)) {
			brightness->priv->hw_changed = TRUE;
			return TRUE;
		}
		return FALSE;
	}

	/* do for each screen */
	length = brightness->priv->resources->len;
	for (i=0; i<length; i++) {
		resource = (XRRScreenResources *) g_ptr_array_index (brightness->priv->resources, i);
		g_debug ("using resource %p", resource);
		ret = gpm_brightness_foreach_resource (brightness, op, resource);
		if (ret)
			success_any = TRUE;
	}
	XSync (brightness->priv->dpy, False);
	return success_any;
}

static void gpm_brightness_may_have_changed (GpmBrightness *brightness);

/**
 * gpm_brightness_filter_xevents:
 **/
static GdkFilterReturn
gpm_brightness_filter_xevents (GdkXEvent *xevent, GdkEvent *event, gpointer data)
{
	GpmBrightness *brightness = GPM_BRIGHTNESS (data);
	if (event->type == GDK_NOTHING)
		return GDK_FILTER_CONTINUE;
	gpm_brightness_may_have_changed (brightness);
	return GDK_FILTER_CONTINUE;
}

static void gpm_brightness_update_cache (GpmBrightness *brightness);

/**
 * gpm_brightness_monitors_changed:
 **/
static void
gpm_brightness_monitors_changed (GdkScreen *screen, GpmBrightness *brightness)
{
	g_return_if_fail (GPM_IS_BRIGHTNESS (brightness));
	gpm_brightness_update_cache (brightness);
}

/**
 * gpm_brightness_update_cache:
 **/
static void
gpm_brightness_update_cache (GpmBrightness *brightness)
{
	guint length;
	Window root;
	GdkScreen *gscreen;
	GdkDisplay *display;
	XRRScreenResources *resource;

	g_return_if_fail (GPM_IS_BRIGHTNESS (brightness));

	/* invalidate and remove all the previous entries */
	length = brightness->priv->resources->len;
	if (length > 0)
		g_ptr_array_set_size (brightness->priv->resources, 0);

	display = gdk_display_get_default ();
	gscreen = gdk_display_get_default_screen (display);

	/* if we have not setup the changed on the monitor, set it here */
	if (g_object_get_data (G_OBJECT (gscreen), "gpk-set-monitors-changed") == NULL) {
		g_debug ("watching ::monitors_changed on %p", gscreen);
		g_object_set_data (G_OBJECT (gscreen), "gpk-set-monitors-changed", (gpointer) "true");
		g_signal_connect (G_OBJECT (gscreen), "monitors_changed",
				  G_CALLBACK (gpm_brightness_monitors_changed), brightness);
	}

	root = RootWindow (brightness->priv->dpy, 0);

	gdk_x11_display_error_trap_push (display);
	resource = XRRGetScreenResourcesCurrent (brightness->priv->dpy, root);
	if (gdk_x11_display_error_trap_pop (display) || resource == NULL) {
		g_warning ("failed to XRRGetScreenResourcesCurrent");
	}

	if (resource != NULL) {
		g_debug ("adding resource %p", resource);
		g_ptr_array_add (brightness->priv->resources, resource);
	}
}

#endif /* HAVE_X11 */

/**
 * gpm_brightness_trust_cache:
 * @brightness: This brightness class instance
 * Return value: if we can trust the cache
 **/
static gboolean
gpm_brightness_trust_cache (GpmBrightness *brightness)
{
	g_return_val_if_fail (GPM_IS_BRIGHTNESS (brightness), FALSE);
	/* only return the cached value if the cache is trusted and we have change events */
	if (brightness->priv->cache_trusted && brightness->priv->has_changed_events) {
		g_debug ("using cache for value %u (okay)", brightness->priv->cache_percentage);
		return TRUE;
	}

	/* can we trust that if we set a value 5 minutes ago, will it still be valid now?
	 * if we have multiple things setting policy on the workstation, e.g. fast user switching
	 * or kpowersave, then this will be invalid -- this logic may be insane */
	if (GPM_SOLE_SETTER_USE_CACHE && brightness->priv->cache_trusted) {
		g_debug ("using cache for value %u (probably okay)", brightness->priv->cache_percentage);
		return TRUE;
	}
	return FALSE;
}

/**
 * gpm_brightness_set:
 * @brightness: This brightness class instance
 * @percentage: The percentage brightness
 * @hw_changed: If the hardware was changed, i.e. the brightness changed
 * Return value: %TRUE if success, %FALSE if there was an error
 **/
gboolean
gpm_brightness_set (GpmBrightness *brightness, guint percentage, gboolean *hw_changed)
{
	gboolean ret = FALSE;
	gboolean trust_cache;

	g_return_val_if_fail (GPM_IS_BRIGHTNESS (brightness), FALSE);

	/* can we check the new value with the cache? */
	trust_cache = gpm_brightness_trust_cache (brightness);
	if (trust_cache && percentage == brightness->priv->cache_percentage) {
		g_debug ("not setting the same value %u", percentage);
		return TRUE;
	}

	/* set the value we want */
#ifdef HAVE_X11
	brightness->priv->shared_value = percentage;
#endif

	/* reset to not-changed */
	brightness->priv->hw_changed = FALSE;

#ifdef HAVE_X11
	ret = gpm_brightness_foreach_screen (brightness, ACTION_BACKLIGHT_SET);
#endif

	/* legacy fallback */
	if (!ret) {
		if (brightness->priv->extension_levels < 0)
			brightness->priv->extension_levels = gpm_brightness_helper_get_value ("get-max-brightness");
		brightness->priv->extension_current = egg_discrete_from_percent (percentage, brightness->priv->extension_levels+1);
		ret = gpm_brightness_helper_set_value ("set-brightness", brightness->priv->extension_current);
	}

	/* did the hardware have to be modified? */
	if (ret && hw_changed != NULL)
		*hw_changed = brightness->priv->hw_changed;

	/* we did something to the hardware, so untrusted */
	if (ret)
		brightness->priv->cache_trusted = FALSE;

#ifdef HAVE_DDCUTIL
	/* ...except over DDC/CI: the monitor answers a read-back with EIO as often as
	 * not on the nVidia driver, so remember what was just written instead of
	 * asking it again and reporting a stale value. */
	if (ret && brightness->priv->use_ddc) {
		brightness->priv->cache_percentage = percentage;
		brightness->priv->cache_trusted = TRUE;
	}
#endif

	return ret;
}

/**
 * gpm_brightness_get:
 * @brightness: This brightness class instance
 * Return value: The percentage brightness, or -1 for no hardware or error
 *
 * Gets the current (or at least what this class thinks is current) percentage
 * brightness. This is quick as no HAL inquiry is done.
 **/
gboolean
gpm_brightness_get (GpmBrightness *brightness, guint *percentage)
{
	gboolean ret = FALSE;
	gboolean trust_cache;
	guint percentage_local;

	g_return_val_if_fail (GPM_IS_BRIGHTNESS (brightness), FALSE);
	g_return_val_if_fail (percentage != NULL, FALSE);

	/* can we use the cache? */
	trust_cache = gpm_brightness_trust_cache (brightness);
	if (trust_cache) {
		*percentage = brightness->priv->cache_percentage;
		return TRUE;
	}

	/* get the brightness from hardware -- slow */
#ifdef HAVE_X11
	ret = gpm_brightness_foreach_screen (brightness, ACTION_BACKLIGHT_GET);
	percentage_local = brightness->priv->shared_value;
#endif

	/* legacy fallback */
	if (!ret) {
		if (brightness->priv->extension_levels < 0)
			brightness->priv->extension_levels = gpm_brightness_helper_get_value ("get-max-brightness");
		brightness->priv->extension_current = gpm_brightness_helper_get_value ("get-brightness");
		percentage_local = egg_discrete_to_percent (brightness->priv->extension_current, brightness->priv->extension_levels+1);
		ret = TRUE;
	}

	/* valid? */
	if (percentage_local > 100) {
		g_warning ("percentage value of %u will be truncated", percentage_local);
		percentage_local = 100;
	}

	/* a new value is always trusted if the method and checks succeed */
	if (ret) {
		brightness->priv->cache_percentage = percentage_local;
		brightness->priv->cache_trusted = TRUE;
		*percentage = percentage_local;
	} else {
		brightness->priv->cache_trusted = FALSE;
	}
	return ret;
}

/**
 * gpm_brightness_up:
 * @brightness: This brightness class instance
 * @hw_changed: If the hardware was changed, i.e. the brightness changed
 * Return value: %TRUE if success, %FALSE if there was an error
 *
 * If possible, put the brightness of the LCD up one unit.
 **/
gboolean
gpm_brightness_up (GpmBrightness *brightness, gboolean *hw_changed)
{
	gboolean ret = FALSE;
	guint step;

	g_return_val_if_fail (GPM_IS_BRIGHTNESS (brightness), FALSE);

	/* reset to not-changed */
	brightness->priv->hw_changed = FALSE;

#ifdef HAVE_X11
	ret = gpm_brightness_foreach_screen (brightness, ACTION_BACKLIGHT_INC);
#endif

	/* did the hardware have to be modified? */
	if (ret && hw_changed != NULL)
		*hw_changed = brightness->priv->hw_changed;

	/* we did something to the hardware, so untrusted */
	if (ret)
		brightness->priv->cache_trusted = FALSE;

	/* legacy fallback */
	if (!ret) {
		if (brightness->priv->extension_levels < 0)
			brightness->priv->extension_levels = gpm_brightness_helper_get_value ("get-max-brightness");
		brightness->priv->extension_current = gpm_brightness_helper_get_value ("get-brightness");

		/* increase by the step, limiting to the maximum possible levels */
		if (brightness->priv->extension_current < brightness->priv->extension_levels) {
			step = gpm_brightness_get_step (brightness->priv->extension_levels);
			brightness->priv->extension_current += step;
			if (brightness->priv->extension_current > brightness->priv->extension_levels)
				brightness->priv->extension_current = brightness->priv->extension_levels;
			ret = gpm_brightness_helper_set_value ("set-brightness", brightness->priv->extension_current);
		}
		if (hw_changed != NULL)
			*hw_changed = ret;
		brightness->priv->cache_trusted = FALSE;
		goto out;
	}
out:
	return ret;
}

/**
 * gpm_brightness_down:
 * @brightness: This brightness class instance
 * @hw_changed: If the hardware was changed, i.e. the brightness changed
 * Return value: %TRUE if success, %FALSE if there was an error
 *
 * If possible, put the brightness of the LCD down one unit.
 **/
gboolean
gpm_brightness_down (GpmBrightness *brightness, gboolean *hw_changed)
{
	gboolean ret = FALSE;
	guint step;

	g_return_val_if_fail (GPM_IS_BRIGHTNESS (brightness), FALSE);

	/* reset to not-changed */
	brightness->priv->hw_changed = FALSE;

#ifdef HAVE_X11
	ret = gpm_brightness_foreach_screen (brightness, ACTION_BACKLIGHT_DEC);
#endif

	/* did the hardware have to be modified? */
	if (ret && hw_changed != NULL)
		*hw_changed = brightness->priv->hw_changed;

	/* we did something to the hardware, so untrusted */
	if (ret)
		brightness->priv->cache_trusted = FALSE;

	/* legacy fallback */
	if (!ret) {
		if (brightness->priv->extension_levels < 0)
			brightness->priv->extension_levels = gpm_brightness_helper_get_value ("get-max-brightness");
		brightness->priv->extension_current = gpm_brightness_helper_get_value ("get-brightness");

		/* decrease by the step, limiting to zero */
		if (brightness->priv->extension_current > 0) {
			step = gpm_brightness_get_step (brightness->priv->extension_levels);
			brightness->priv->extension_current -= step;
			if (brightness->priv->extension_current < 0)
				brightness->priv->extension_current = 0;
			ret = gpm_brightness_helper_set_value ("set-brightness", brightness->priv->extension_current);
		}
		if (hw_changed != NULL)
			*hw_changed = ret;
		brightness->priv->cache_trusted = FALSE;
		goto out;
	}
out:
	return ret;
}

/**
 * gpm_brightness_may_have_changed:
 **/
static void
gpm_brightness_may_have_changed (GpmBrightness *brightness)
{
	gboolean ret;
	guint percentage;
	ret = gpm_brightness_get (brightness, &percentage);
	if (!ret) {
		g_warning ("failed to get output");
		return;
	}
	g_debug ("emitting brightness-changed (%u)", percentage);
	g_signal_emit (brightness, signals [BRIGHTNESS_CHANGED], 0, percentage);
}

/**
 * gpm_brightness_has_hw:
 **/
gboolean
gpm_brightness_has_hw (GpmBrightness *brightness)
{
	g_return_val_if_fail (GPM_IS_BRIGHTNESS (brightness), FALSE);

	/* use XRandR first */
#ifdef HAVE_X11
	if (brightness->priv->has_extension)
		return TRUE;
#endif

	/* fallback to legacy access */
	if (brightness->priv->extension_levels < 0)
		brightness->priv->extension_levels = gpm_brightness_helper_get_value ("get-max-brightness");
	if (brightness->priv->extension_levels > 0)
		return TRUE;
	return FALSE;
}

/**
 * gpm_brightness_finalize:
 **/
static void
gpm_brightness_finalize (GObject *object)
{
	GpmBrightness *brightness;
	g_return_if_fail (object != NULL);
	g_return_if_fail (GPM_IS_BRIGHTNESS (object));
	brightness = GPM_BRIGHTNESS (object);
#ifdef HAVE_DDCUTIL
	if (brightness->priv->ddc_apply_idle != 0) {
		g_source_remove (brightness->priv->ddc_apply_idle);
		brightness->priv->ddc_apply_idle = 0;
	}
	if (brightness->priv->ddc_handle != NULL) {
		ddca_close_display (brightness->priv->ddc_handle);
		brightness->priv->ddc_handle = NULL;
	}
#endif
#ifdef HAVE_X11
	if (brightness->priv->resources != NULL)
		g_ptr_array_unref (brightness->priv->resources);
	if (brightness->priv->root_window != NULL)
		gdk_window_remove_filter (brightness->priv->root_window,
					  gpm_brightness_filter_xevents, brightness);
#endif
	G_OBJECT_CLASS (gpm_brightness_parent_class)->finalize (object);
}

/**
 * gpm_brightness_class_init:
 **/
static void
gpm_brightness_class_init (GpmBrightnessClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gpm_brightness_finalize;

	signals [BRIGHTNESS_CHANGED] =
		g_signal_new ("brightness-changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GpmBrightnessClass, brightness_changed),
			      NULL, NULL, g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE, 1, G_TYPE_UINT);
}

/**
 * gpm_brightness_init:
 * @brightness: This brightness class instance
 **/
static void
gpm_brightness_init (GpmBrightness *brightness)
{
	brightness->priv = gpm_brightness_get_instance_private (brightness);

	brightness->priv->cache_trusted = FALSE;
	brightness->priv->has_changed_events = FALSE;
	brightness->priv->cache_percentage = 0;
	brightness->priv->hw_changed = FALSE;
	brightness->priv->extension_levels = -1;

#ifdef HAVE_X11
	if (gdk_display_get_default () != NULL &&
	    GDK_IS_X11_DISPLAY (gdk_display_get_default ())) {
		GdkScreen *screen;
		GdkDisplay *display;
		int event_base;
		int ignore;

		brightness->priv->resources = g_ptr_array_new_with_free_func ((GDestroyNotify) XRRFreeScreenResources);

		/* can we do this */
		brightness->priv->has_extension = gpm_brightness_setup_display (brightness);
		if (brightness->priv->has_extension == FALSE)
			g_debug ("no XRANDR extension");

		screen = gdk_screen_get_default ();
		brightness->priv->root_window = gdk_screen_get_root_window (screen);
		display = gdk_display_get_default ();

		/* as we a filtering by a window, we have to add an event type */
		if (!XRRQueryExtension (GDK_DISPLAY_XDISPLAY (gdk_display_get_default()), &event_base, &ignore)) {
			g_warning ("can't get event_base for XRR");
		}
		gdk_x11_register_standard_event_type (display, event_base, RRNotify + 1);
		gdk_window_add_filter (brightness->priv->root_window,
				       gpm_brightness_filter_xevents, brightness);

		/* don't abort on error */
		gdk_x11_display_error_trap_push (display);
		XRRSelectInput (GDK_DISPLAY_XDISPLAY (gdk_display_get_default()),
				GDK_WINDOW_XID (brightness->priv->root_window),
				RRScreenChangeNotifyMask |
				RROutputPropertyNotifyMask); /* <--- the only one we need, but see rh:345551 */
		gdk_display_flush (display);
		if (gdk_x11_display_error_trap_pop (display))
			g_warning ("failed to select XRRSelectInput");

		/* create cache of XRRScreenResources as XRRGetScreenResources() is slow */
		gpm_brightness_update_cache (brightness);
		return;
	}
#endif
	g_debug ("No XRandR brightness backend; using sysfs fallback only");
}

/**
 * gpm_brightness_new:
 * Return value: A new brightness class instance.
 * Can return NULL if no suitable hardware is found.
 **/
GpmBrightness *
gpm_brightness_new (void)
{
	if (gpm_brightness_object != NULL) {
		g_object_ref (gpm_brightness_object);
	} else {
		gpm_brightness_object = g_object_new (GPM_TYPE_BRIGHTNESS, NULL);
		g_object_add_weak_pointer (gpm_brightness_object, &gpm_brightness_object);
	}
	return GPM_BRIGHTNESS (gpm_brightness_object);
}
