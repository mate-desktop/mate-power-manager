# configuration
conf_data = configuration_data()
conf_data.set('version', meson.project_version())
conf_data.set('sysconfdir', sysconfdir_path)
conf_data.set('datadir', datadir_path)
conf_data.set('bindir', bindir_path)
conf_data.set('sbindir', sbindir_path)
conf_data.set('localedir', localedir_path)
conf_data.set('iconsdir', iconsdir_path)
conf_data.set('gettext_package', meson.project_name())
conf_data.set('WITH_KEYRING', get_option('keyring').enabled())

configure_file(input: 'config.h.in', output: 'config.h', configuration: conf_data)

gpm_marshal = gnome.genmarshal('gpm-marshal', sources: 'gpm-marshal.list', prefix: 'gpm_marshal')

dbus_binding_powermanager = custom_target('org.mate.PowerManager.h',
  input: 'org.mate.PowerManager.xml',
  output: 'org.mate.PowerManager.h',
  command: [dbus_binding_tool, '--prefix=gpm_manager', '--mode=glib-server', '--output=@OUTPUT@', '@INPUT@']
)

dbus_binding_backlight = custom_target('org.mate.PowerManager.Backlight.h',
  input: 'org.mate.PowerManager.Backlight.xml',
  output: 'org.mate.PowerManager.Backlight.h',
  command: [dbus_binding_tool, '--prefix=gpm_backlight', '--mode=glib-server', '--output=@OUTPUT@', '@INPUT@']
)

dbus_binding_kbdbacklight = custom_target('org.mate.PowerManager.KbdBacklight.h',
  input: 'org.mate.PowerManager.KbdBacklight.xml',
  output: 'org.mate.PowerManager.KbdBacklight.h',
  command: [dbus_binding_tool, '--prefix=gpm_kbd_backlight', '--mode=glib-server', '--output=@OUTPUT@', '@INPUT@']
)

statics_resources = gnome.compile_resources(
  'mate-power-statistics-resources',
  '../data/org.mate.power-manager.statistics.gresource.xml',
  c_name: 'statistics',
  source_dir: '../data'
)

preferences_resources = gnome.compile_resources(
  'mate-power-preferences-resources',
  '../data/org.mate.power-manager.preferences.gresource.xml',
  c_name: 'preferences',
  source_dir: '../data'
)

power_manager_resources = gnome.compile_resources(
  'mate-power-manager-resources',
  '../data/org.mate.power-manager.manager.gresource.xml',
  c_name: 'preferences',
  source_dir: '../data'
)

src_libgpmshared_a = [
  gpm_marshal,
  dbus_binding_powermanager,
  dbus_binding_backlight,
  dbus_binding_kbdbacklight,
  'egg-color.c',
  'egg-color.h',
  'egg-precision.h',
  'egg-precision.c',
  'egg-array-float.c',
  'egg-array-float.h',
  'egg-idletime.h',
  'egg-idletime.c',
  'egg-discrete.h',
  'egg-discrete.c',
  'egg-console-kit.h',
  'egg-console-kit.c',
  'gpm-common.h',
  'gpm-common.c',
  'gpm-brightness.h',
  'gpm-brightness.c',
  'gpm-upower.c',
  'gpm-upower.h',
]

mpm_static_lib = static_library('libgpmshared', src_libgpmshared_a, dependencies: dependencies)

executable('mate-power-backlight-helper', 'gpm-backlight-helper.c', dependencies: dependencies, link_with: [mpm_static_lib])

executable(
  'mate-power-statistics',
  [
    'gpm-statistics.c',
    'gpm-point-obj.c',
    'gpm-point-obj.h',
    'gpm-graph-widget.h',
    'gpm-graph-widget.c'
  ],
  dependencies: dependencies,
  link_with: [mpm_static_lib]
)

executable(
  'mate-power-preferences',
  [
    preferences_resources,
    'gpm-prefs.c',
    'gpm-prefs-core.h',
    'gpm-prefs-core.c'
  ],
  dependencies: dependencies,
  link_with: [mpm_static_lib]
)

executable(
  'mate-power-manager',
  [
    power_manager_resources,
    'gpm-dpms.h',
    'gpm-dpms.c',
	  'gpm-phone.h',
	  'gpm-phone.c',
	  'gpm-backlight.h',
	  'gpm-backlight.c',
	  'gpm-idle.h',
	  'gpm-idle.c',
	  'gpm-load.h',
	  'gpm-load.c',
	  'gpm-control.h',
	  'gpm-control.c',
	  'gpm-button.h',
	  'gpm-button.c',
	  'gpm-kbd-backlight.h',
	  'gpm-kbd-backlight.c',
	  'gpm-main.c',
	  'gpm-manager.h',
	  'gpm-manager.c',
	  'gpm-tray-icon.h',
	  'gpm-tray-icon.c',
	  'gpm-screensaver.h',
	  'gpm-screensaver.c',
	  'gpm-session.h',
	  'gpm-session.c',
	  'gpm-networkmanager.h',
	  'gpm-networkmanager.c',
	  'gpm-icon-names.h',
	  'gsd-media-keys-window.h',
	  'gsd-media-keys-window.c',
	  'msd-osd-window.h',
	  'msd-osd-window.c',
	  'gpm-engine.h',
	  'gpm-engine.c',
  ],
  dependencies: dependencies,
  link_with: [mpm_static_lib],
  install : true)
