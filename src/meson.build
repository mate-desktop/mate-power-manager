sources = files(
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
  'gpm-upower.h'
)

cflags = [
  '-DMATELOCALEDIR="@0@"'.format(matelocaledir),
  '-DG_LOG_DOMAIN="PowerManager"',
  '-DGPM_ICONS_DATA="@0@"'.format(mateicons),
  '-DINHIBIT_MENU_UI_DIR="@0@"'.format(mateui),
  '-DSBINDIR="@0@"'.format(matesbindir),
  '-DBINDIR="@0@"'.format(matebindir),
  '-DGTKBUILDERDIR="@0@"'.format(pkgdatadir),
  '-DEGG_VERBOSE="GPM_VERBOSE"',
  '-DEGG_LOGGING="GPM_LOGGING"',
  '-DUP_DISABLE_DEPRECATED',
  '-DI_KNOW_THE_DEVICEKIT_POWER_API_IS_SUBJECT_TO_CHANGE',
  '-DEGG_CONSOLE="GPM_CONSOLE"'
]
marshal_files = gnome.genmarshal('gpm-marshal', prefix : 'gpm_marshal', sources : 'gpm-marshal.list')

dbus_binding_tool = find_program('dbus-binding-tool')
dbus_Backlight = custom_target('org.mate.PowerManager.Backlight',
  input: 'org.mate.PowerManager.Backlight.xml',
  output: 'org.mate.PowerManager.Backlight.h',
  command: [dbus_binding_tool, '--prefix=gpm_backlight', '--mode=glib-server',
            '--output=@OUTPUT@', '@INPUT@']
)
dbus_KbdBacklight = custom_target('org.mate.PowerManager.KbdBacklight',
  input: 'org.mate.PowerManager.KbdBacklight.xml',
  output: 'org.mate.PowerManager.KbdBacklight.h',
  command: [dbus_binding_tool, '--prefix=gpm_kbd_backlight', '--mode=glib-server',
            '--output=@OUTPUT@', '@INPUT@']
)
dbus_Manager = custom_target('org.mate.PowerManager',
  input: 'org.mate.PowerManager.xml',
  output: 'org.mate.PowerManager.h',
  command: [dbus_binding_tool, '--prefix=gpm_manager', '--mode=glib-server',
            '--output=@OUTPUT@', '@INPUT@']
)
deps = [
  glib,
  gtk,
  dbusglib,
  cairo,
  upower,
  keyring,
  libsecret,
  notify,
]

libmpm_shared = static_library(
  'libmpmstatic',
  sources: [
    sources,
    marshal_files
  ],
  include_directories: config_inc,
  dependencies: deps,
  c_args: cflags,
  #link_args: ldflags,
  install: false,
)
executable(
  'mate-power-backlight-helper',
  sources : 'gpm-backlight-helper.c',
  include_directories: config_inc,
  dependencies : deps,
  link_with :libmpm_shared,
  c_args : cflags,
  install : true,
  install_dir : get_option('sbindir')
)

mate_power_statistics_resources = gnome.compile_resources(
  'mate-power-statistics-resources',
  join_paths(data_dir,'org.mate.power-manager.statistics.gresource.xml'),
  source_dir : data_dir,
  c_name : 'statistics'
)

executable(
  'mate-power-statistics',
  mate_power_statistics_resources,
  sources : [
    'gpm-point-obj.c',
    'gpm-statistics.c',
    'gpm-graph-widget.c',
  ],
  include_directories : [
    include_directories('..'),
  ],
  dependencies : [
    libm,
    deps
  ],
  link_with :libmpm_shared,
  c_args : cflags,
  install : true,
  install_dir : get_option('bindir')
)

mate_power_preferences_resources = gnome.compile_resources(
  'mate-power-preferences-resources',
  join_paths(data_dir,'org.mate.power-manager.preferences.gresource.xml'),
  source_dir : data_dir,
  c_name : 'preferences'
)

executable(
  'mate-power-preferences',
  mate_power_preferences_resources,
  sources : [
    'gpm-prefs.c',
    'gpm-prefs-core.c',
  ],
  include_directories : [
    include_directories('..'),
  ],
  dependencies : [
    libm,
    x11,
    xrandr,
    deps
  ],
  link_with :libmpm_shared,
  c_args : cflags,
  install : true,
  install_dir : get_option('bindir')
)
mate_power_manager_resources = gnome.compile_resources(
  'mate-power-manager-resources',
  join_paths(data_dir,'org.mate.power-manager.manager.gresource.xml'),
  source_dir : data_dir,
  c_name : 'manager'
)

executable(
  'mate-power-manager',
  mate_power_manager_resources,
  sources : [
    'gpm-dpms.c',
    'gpm-phone.c',
    'gpm-backlight.c',
    'gpm-idle.c',
    'gpm-load.c',
    'gpm-control.c',
    'gpm-button.c',
    'gpm-kbd-backlight.c',
    'gpm-main.c',
    'gpm-manager.c',
    'gpm-tray-icon.c',
    'gpm-screensaver.c',
    'gpm-session.c',
    'gpm-networkmanager.c',
    'gsd-media-keys-window.c',
    'msd-osd-window.c',
    'gpm-engine.c',
    dbus_Backlight,
    dbus_KbdBacklight,
    dbus_Manager,
  ],
  include_directories : [
    include_directories('..'),
  ],
  dependencies : [
    libm,
    md,
    x11,
    xext,
    xrandr,
    canberra,
    deps
  ],
  link_with :libmpm_shared,
  c_args : cflags,
  install : true,
  install_dir : get_option('bindir')
)

if get_option('enable-tests')
  e = executable(
    'mate-power-self-test',
    sources : [
      'gpm-self-test.c',
      'egg-color.c',
      'egg-test.c',
      'egg-precision.c',
      'egg-precision.c',
      'egg-idletime.c',
      'egg-discrete.c',
      'egg-array-float.c',
      'egg-console-kit.c',
      'gpm-control.c',
      'gpm-networkmanager.c',
      'gpm-dpms.c',
      'gpm-button.c',
      'gpm-screensaver.c',
      'gpm-engine.c',
      'gpm-phone.c',
      'gpm-idle.c',
      'gpm-session.c',
      'gpm-load.c',
      'gpm-common.c',
      'gpm-upower.c',
      marshal_files,
    ],
    include_directories : [
      include_directories('..'),
    ],
    dependencies : [
      libm,
      x11,
      xext,
      xrandr,
      deps
    ],
    link_with :libmpm_shared,
    c_args : [
      test_args,
      '-DEGG_TEST'
    ]
  )
  test('mate-power-self-test', e)
endif
