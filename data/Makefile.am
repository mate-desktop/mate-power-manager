SUBDIRS = icons


@INTLTOOL_DESKTOP_RULE@
desktopdir = $(datadir)/applications
desktop_in_files = mate-power-preferences.desktop.in \
		   mate-power-statistics.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@
autostartdir = $(sysconfdir)/xdg/autostart
autostart_in_files = mate-power-manager.desktop.in
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)

servicedir       = $(DBUS_SERVICES_DIR)
service_in_files = mate-power-manager.service.in
service_DATA     = $(service_in_files:.service.in=.service)

$(service_DATA): $(service_in_files) Makefile
	@sed -e "s|\@servicedir\@|$(bindir)|" $< > $@

@GSETTINGS_RULES@
@INTLTOOL_XML_NOMERGE_RULE@
gsettings_schemas_in_files = org.mate.power-manager.gschema.xml.in
gsettings_SCHEMAS = $(gsettings_schemas_in_files:.gschema.xml.in=.gschema.xml)

convertdir = $(datadir)/MateConf/gsettings
convert_DATA = mate-power-manager.convert

pkgdata_DATA =						\
	gpm-statistics.ui				\
	gpm-statistics-deprecated.ui	\
	gpm-feedback-widget.ui			\
	gpm-prefs.ui					\
	gpm-prefs-deprecated.ui			\
	acme.ui

EXTRA_DIST =						\
	$(gsettings_SCHEMAS)				\
	$(service_in_files)				\
	$(autostart_in_files)				\
	$(desktop_in_files)				\
	$(gsettings_schemas_in_files)	\
	$(convert_DATA)					\
	$(pkgdata_DATA)

clean-local :
	rm -f *~

DISTCLEANFILES =					\
	mate-power-manager.desktop			\
	mate-power-preferences.desktop			\
	mate-power-statistics.desktop			\
	mate-power-manager.service		\
	org.mate.power-manager.gschema.xml
