po_dir = join_paths(meson.source_root(), 'po')

source_files = files(
  'gpm-common.c',
  'inhibit-applet.c'
)

cflags = [
  '-DMATELOCALEDIR="@0@"'.format(matelocaledir),
  '-DG_LOG_DOMAIN="~/mate-power-manager.log"',
  '-DGPM_ICONS_DATA="@0@"'.format(mateicons),
  '-DINHIBIT_MENU_UI_DIR="@0@"'.format(mateui),
  '-DEGG_VERBOSE="GPM_VERBOSE"',
  '-DEGG_LOGGING="GPM_LOGGING"',
  '-DEGG_CONSOLE="GPM_CONSOLE"'
  ]

executable('mate-inhibit-applet',
  sources: source_files,
  dependencies : [
    gtk,
    glib,
    cairo,
    notify,
    dbus,
    dbusglib,
    matepanel
  ],
  include_directories : config_inc,
  c_args : cflags,
  install : true,
  install_dir: mateexecdir,
)

# .service files

service_data = configuration_data()
service_data.set('LIBEXECDIR', join_paths(prefix, get_option('libexecdir')))

configure_file(
  input : 'org.mate.panel.applet.InhibitAppletFactory.service.in',
  output : 'org.mate.panel.applet.InhibitAppletFactory.service',
  configuration : service_data,
  install : true,
  install_dir : join_paths(matedatadir, 'dbus-1/services')
)

desktop_data = configuration_data()
desktop_data.set('VERSION', meson.project_version())
desktop_data.set('LIBEXECDIR', join_paths(prefix, get_option('libexecdir')))
desktop_in_file = configure_file(
  input : 'org.mate.InhibitApplet.mate-panel-applet.desktop.in.in',
  output : 'org.mate.InhibitApplet.mate-panel-applet.desktop.in',
  configuration : desktop_data
)

i18n.merge_file(
  input : desktop_in_file,
  output : 'org.mate.InhibitApplet.mate-panel-applet',
  type : 'desktop',
  po_dir : po_dir,
  install : true,
  install_dir : join_paths(matedatadir, 'mate-panel', 'applets')
)

install_data('inhibit-applet-menu.xml',
  install_dir : join_paths(pkgdatadir, 'ui')
)
