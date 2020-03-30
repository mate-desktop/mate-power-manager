preferences_desktop_data = configuration_data()
preferences_desktop_data.set('VERSION', meson.project_version())

preferences_desktop = configure_file(
  input: 'mate-power-preferences.desktop.in.in',
  output: 'mate-power-preferences.desktop.in',
  configuration: preferences_desktop_data
)

statistics_desktop_data = configuration_data()
statistics_desktop_data.set('VERSION', meson.project_version())

statistics_desktop = configure_file(
  input: 'mate-power-statistics.desktop.in.in',
  output: 'mate-power-statistics.desktop.in',
  configuration: statistics_desktop_data
)

mpm_autostart_data = configuration_data()
mpm_autostart_data.set('VERSION', meson.project_version())

mpm_autostart = configure_file(
  input: 'mate-power-manager.desktop.in.in',
  output: 'mate-power-manager.desktop.in',
  configuration: mpm_autostart_data
)

mpm_service_data = configuration_data()
mpm_service_data.set('servicedir', servicedir_path)

mpm_service = configure_file(
  input: 'org.mate.PowerManager.service.in',
  output: 'org.mate.PowerManager.service',
  configuration: mpm_service_data,
  install: true,
  install_dir: servicedir_path
)

mpm_gsettings_schema_data = configuration_data()
mpm_gsettings_schema_data.set('GETTEXT_PACKAGE', meson.project_name())

mpm_gsettings_schema = configure_file(
  input: 'org.mate.power-manager.gschema.xml.in',
  output: 'org.mate.power-manager.gschema.xml',
  configuration: mpm_gsettings_schema_data,
  install: true,
  install_dir: join_paths(datadir_path, 'glib-2.0', 'schemas')
)
meson.add_install_script('meson_post_install.py')

i18n.merge_file(
  input: preferences_desktop,
  output: 'mate-power-preferences.desktop',
  type: 'desktop',
  po_dir: podir,
  install: true,
  install_dir: desktopdir_path
)

i18n.merge_file(
  input: statistics_desktop,
  output: 'mate-power-statistics.desktop',
  type: 'desktop',
  po_dir: podir,
  install: true,
  install_dir: desktopdir_path
)

i18n.merge_file(
  input: mpm_autostart,
  output: 'mate-power-manager.desktop',
  type: 'desktop',
  po_dir: podir,
  install: true,
  install_dir: join_paths(sysconfdir_path, 'xdg', 'autostart')
)

install_man(
  [
    'mate-power-manager.1',
    'mate-power-backlight-helper.1',
    'mate-power-statistics.1',
    'mate-power-preferences.1'
  ],
  install_dir: mandir_path
)

install_data(
  ['acme.ui'],
  install_dir: pkgdatadir_path
)

subdir('icons')
