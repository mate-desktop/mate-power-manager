NULL =

AM_CPPFLAGS =							\
	-I. -I$(srcdir) 					\
	$(GLIB_CFLAGS)						\
	$(DBUS_CFLAGS)						\
	$(CAIRO_CFLAGS)						\
	$(PANEL_CFLAGS)						\
	$(LIBNOTIFY_CFLAGS)					\
	-DBINDIR=\"$(bindir)\"			 		\
	-DMATELOCALEDIR=\""$(datadir)/locale"\"			\
	-DDATADIR=\"$(datadir)\"				\
	-DPREFIX=\""$(prefix)"\" 				\
	-DSYSCONFDIR=\""$(sysconfdir)"\" 			\
	-DLIBDIR=\""$(libdir)"\" 				\
	-DVERSION="\"$(VERSION)\"" 				\
	-DGPM_ICONS_DATA=\"$(pkgdatadir)/icons\"		\
	-DPOWER_PROFILES_MENU_UI_DIR=\""$(uidir)"\"		\
	-DEGG_LOG_FILE=\""~/mate-power-manager.log"\"		\
	-DEGG_VERBOSE="\"GPM_VERBOSE\""				\
	-DEGG_LOGGING="\"GPM_LOGGING\""				\
	-DEGG_CONSOLE="\"GPM_CONSOLE\""				\
	$(DISABLE_DEPRECATED)					\
	$(WARN_CFLAGS)						\
	$(NULL)

if APPLETS_INPROCESS
APPLET_IN_PROCESS = true
APPLET_LOCATION   = $(libdir)/$(PACKAGE)/libmate-power-profiles-applet.so

pkglib_LTLIBRARIES = libmate-power-profiles-applet.la
libmate_power_profiles_applet_la_SOURCES =			\
	power-profiles-applet.c					\
	gpm-common.c						\
	gpm-common.h
libmate_power_profiles_applet_la_LDFLAGS = -module -avoid-version
libmate_power_profiles_applet_la_LIBADD =			\
	$(DBUS_LIBS)						\
	$(CAIRO_LIBS)						\
	$(PANEL_LIBS)
else
APPLET_IN_PROCESS = false
APPLET_LOCATION   = $(libexecdir)/mate-power-profiles-applet

libexec_PROGRAMS = mate-power-profiles-applet
mate_power_profiles_applet_SOURCES =				\
	power-profiles-applet.c					\
	gpm-common.c						\
	gpm-common.h
mate_power_profiles_applet_LDADD =				\
	$(DBUS_LIBS)						\
	$(CAIRO_LIBS)						\
	$(PANEL_LIBS)
endif

appletdir       = $(datadir)/mate-panel/applets
applet_in_files = org.mate.PowerProfilesApplet.mate-panel-applet.desktop.in
applet_DATA     = $(applet_in_files:.mate-panel-applet.desktop.in=.mate-panel-applet)

$(applet_DATA): $(applet_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --keyword=Name --keyword=Description --template $< -d $(top_srcdir)/po -o $@

$(applet_in_files): $(applet_in_files).in Makefile
	$(AM_V_GEN) sed \
            -e "s|\@APPLET_LOCATION\@|$(APPLET_LOCATION)|" \
            -e "s|\@APPLET_IN_PROCESS\@|$(APPLET_IN_PROCESS)|" \
            -e "s|\@VERSION\@|$(PACKAGE_VERSION)|" \
            $< > $@

if !APPLETS_INPROCESS
servicedir       = $(datadir)/dbus-1/services
service_in_files = org.mate.panel.applet.PowerProfilesAppletFactory.service.in
service_DATA     = $(service_in_files:.service.in=.service)

org.mate.panel.applet.PowerProfilesAppletFactory.service: $(service_in_files)
	$(AM_V_GEN)sed \
            -e "s|\@LIBEXECDIR\@|$(libexecdir)|" \
            $< > $@
endif

uidir   = $(datadir)/mate-power-manager/ui
ui_DATA = power-profiles-applet-menu.xml

EXTRA_DIST = $(applet_in_files).in $(ui_DATA) $(service_in_files)

DISTCLEANFILES =						\
	$(applet_DATA) $(applet_DATA).in $(service_DATA)

-include $(top_srcdir)/git.mk
