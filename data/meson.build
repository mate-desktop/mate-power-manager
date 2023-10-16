po_dir = join_paths(meson.source_root(), 'po')
man1_dir = join_paths(matemandir, 'man1')
subdir('icons')

schema_data = configuration_data()
schema_data.set('GETTEXT_PACKAGE', meson.project_name())
configure_file(
  input : 'org.mate.power-manager.gschema.xml.in',
  output : 'org.mate.power-manager.gschema.xml',
  configuration : schema_data,
  install : true,
  install_dir : join_paths(matedatadir, 'glib-2.0', 'schemas')
)

# .service files

service_data = configuration_data()
service_data.set('servicedir', matebindir)

configure_file(
  input : 'org.mate.PowerManager.service.in',
  output : 'org.mate.PowerManager.service',
  configuration : service_data,
  install : true,
  install_dir : join_paths(matedatadir, 'dbus-1/services')
)

# .desktop file

desktop_data = configuration_data()
desktop_data.set('VERSION', meson.project_version())
desktop_in_file = configure_file(
  input : 'mate-power-statistics.desktop.in.in',
  output : 'mate-power-statistics.desktop.in',
  configuration : desktop_data
)

i18n.merge_file(
  input : desktop_in_file,
  output : 'mate-power-statistics.desktop',
  type : 'desktop',
  po_dir : po_dir,
  install : true,
  install_dir : join_paths(matedatadir, 'applications')
)

desktop_in_file = configure_file(
  input : 'mate-power-preferences.desktop.in.in',
  output : 'mate-power-preferences.desktop.in',
  configuration : desktop_data
)

i18n.merge_file(
  input : desktop_in_file,
  output : 'mate-power-preferences.desktop',
  type : 'desktop',
  po_dir : po_dir,
  install : true,
  install_dir : join_paths(matedatadir, 'applications')
)

desktop_in_file = configure_file(
  input : 'mate-power-manager.desktop.in.in',
  output : 'mate-power-manager.desktop.in',
  configuration : desktop_data
)

i18n.merge_file(
  input : desktop_in_file,
  output : 'mate-power-manager.desktop',
  type : 'desktop',
  po_dir : po_dir,
  install : true,
  install_dir: join_paths(get_option('sysconfdir'), 'xdg', 'autostart')
)

install_data('mate-power-statistics.1',
  install_dir : man1_dir
)
install_data('mate-power-preferences.1',
  install_dir : man1_dir
)
install_data('mate-power-manager.1',
  install_dir : man1_dir
)
install_data('mate-power-backlight-helper.1',
  install_dir : man1_dir
)

install_data('acme.ui',
  install_dir : pkgdatadir
)
