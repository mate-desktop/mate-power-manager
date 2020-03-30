conf_data = configuration_data()
conf_data.set('version', meson.project_version())
conf_data.set('sysconfdir', sysconfdir_path)
conf_data.set('datadir', datadir_path)
conf_data.set('bindir', bindir_path)
conf_data.set('sbindir', sbindir_path)
conf_data.set('localedir', localedir_path)
conf_data.set('iconsdir', iconsdir_path)
conf_data.set('uidir', uidir_path)
conf_data.set('gettext_package', meson.project_name())

configure_file(input: 'config.h.in', output: 'config.h', configuration: conf_data)

executable(
  'mate-brightness-applet',
  [
    'brightness-applet.c',
    'gpm-common.c',
  ],
  dependencies: dependencies,
  install: true,
  install_dir: libexecdir_path
)

desktop_data = configuration_data()
desktop_data.set('LIBEXECDIR', libexecdir_path)
desktop_data.set('VERSION', meson.project_version())

desktop_in = configure_file(
  input: 'org.mate.BrightnessApplet.mate-panel-applet.desktop.in.in',
  output: 'org.mate.BrightnessApplet.mate-panel-applet.desktop.in',
  configuration: desktop_data 
)

i18n.merge_file(
  input: desktop_in,
  output: 'org.mate.BrightnessApplet.mate-panel-applet.desktop',
  type: 'desktop',
  po_dir: podir,
  install: true,
  install_dir: appletdir_path,
)

service_data = configuration_data()
service_data.set('LIBEXECDIR', libexecdir_path)

service_file = configure_file(
  input: 'org.mate.panel.applet.BrightnessAppletFactory.service.in',
  output: 'org.mate.panel.applet.BrightnessAppletFactory.service',
  configuration: service_data,
  install: true,
  install_dir: servicedir_path
)

install_data(
  ['brightness-applet-menu.xml'],
  install_dir: uidir_path
)

