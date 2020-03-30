project('mate-power-manager', 'c', version : '1.24.0')

gnome = import('gnome')
dbus_binding_tool = find_program('dbus-binding-tool')

i18n = import('i18n')
add_project_arguments(
  [
    '-DGETTEXT_PACKAGE="' + meson.project_name() + '"',
    '-DHAVE_CONFIG_H'
  ],
  language: 'c'
)

cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required: false)

dependencies = [
  m_dep,
  dependency('glib-2.0', version: '>=2.50.0'),
  dependency('gobject-2.0'),
  dependency('gio-2.0 ', version: '>=2.50.0'),
  dependency('gio-unix-2.0'),
  dependency('libcanberra-gtk3', version: '>=0.10'),
  dependency('dbus-glib-1', version: '>=0.70'),
  dependency('dbus-1', version: '>=1.0'),
  dependency('gthread-2.0',),
  dependency('gtk+-3.0', version: '>=3.22.0'),
  dependency('cairo', version: '>=1.0.0'),
  dependency('gdk-3.0', version: '>=3.22.0'),
  dependency('gdk-x11-3.0', version: '>=3.22.0'),
  dependency('xrandr', version: '>=1.3.0'),
  dependency('x11'),
  dependency('xext'),
  dependency('xproto', version: '>=7.0.15'),
  dependency('libnotify', version: '>=0.7.0'),
  dependency('upower-glib', version: '>=0.99.8'),
  dependency('gnome-keyring-1', version: '>=3.0.0', required: get_option('keyring')),
]

# defult paths
prefix_path = get_option('prefix')
sysconfdir_path = join_paths(prefix_path, get_option('sysconfdir'))
datadir_path = join_paths(prefix_path, get_option('datadir'))
pkgdatadir_path = join_paths(datadir_path, meson.project_name())
bindir_path = join_paths(prefix_path, get_option('bindir'))
sbindir_path = join_paths(prefix_path, get_option('sbindir'))
libexecdir_path = join_paths(prefix_path, get_option('libexecdir'))
localedir_path = join_paths(prefix_path, get_option('localedir'))
iconsdir_path = join_paths(datadir_path, 'icons')
uidir_path = join_paths(pkgdatadir_path, 'ui')
servicedir_path = join_paths(datadir_path, 'dbus-1', 'services')
appletdir_path = join_paths(datadir_path, 'mate-panel', 'applets')
desktopdir_path = join_paths(datadir_path, 'applications')
mandir_path = join_paths(prefix_path, get_option('mandir'))

podir = join_paths(meson.source_root(), 'po')

if get_option('applets').enabled()
  dependencies += dependency('libmatepanelapplet-4.0')

  subdir('applets')
endif

subdir('data')
subdir('help')
subdir('po')
subdir('policy')
subdir('src')
