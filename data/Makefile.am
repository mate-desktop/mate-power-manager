SUBDIRS = icons


desktopdir = $(datadir)/applications
desktop_in_files = mate-power-preferences.desktop.in \
		   mate-power-statistics.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
%.desktop: %.desktop.in
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

autostartdir = $(sysconfdir)/xdg/autostart
autostart_in_files = mate-power-manager.desktop.in
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)
$(autostart_DATA): $(autostart_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

servicedir       = $(DBUS_SERVICES_DIR)
service_in_files = org.mate.PowerManager.service.in
service_DATA     = $(service_in_files:.service.in=.service)

$(service_DATA): $(service_in_files) Makefile
	@sed -e "s|\@servicedir\@|$(bindir)|" $< > $@

@GSETTINGS_RULES@
gsettings_schemas_in_files = org.mate.power-manager.gschema.xml.in
gsettings_SCHEMAS = $(gsettings_schemas_in_files:.xml.in=.xml)

pkgdata_DATA =							\
	acme.ui

man_MANS =							\
	mate-power-manager.1					\
	mate-power-backlight-helper.1				\
	mate-power-statistics.1					\
	mate-power-preferences.1

EXTRA_DIST =							\
	$(service_in_files)					\
	$(autostart_in_files)					\
	$(desktop_in_files)					\
	$(gsettings_schemas_in_files)				\
	$(pkgdata_DATA)						\
	$(man_MANS)						\
	org.mate.power-manager.manager.gresource.xml		\
	org.mate.power-manager.preferences.gresource.xml	\
	org.mate.power-manager.statistics.gresource.xml		\
	gpm-prefs.ui						\
	gpm-statistics.ui					\
	mate-power-manager.about

clean-local :
	rm -f *~

DISTCLEANFILES =						\
	mate-power-manager.desktop				\
	mate-power-preferences.desktop				\
	mate-power-statistics.desktop				\
	org.mate.PowerManager.service				\
	$(gsettings_SCHEMAS)
