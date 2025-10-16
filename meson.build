project('mate-power-manager', 'c',
  version : '1.28.1',
  default_options : ['warning_level=1'],
  meson_version : '>=0.50.0'
)

conf = configuration_data()

prefix = get_option('prefix')
mateexecdir = join_paths(prefix, get_option('libexecdir'))
matedatadir = join_paths(prefix, get_option('datadir'))
matelocaledir = join_paths(prefix, get_option('localedir'))
matebindir = join_paths(prefix, get_option('bindir'))
matesbindir = join_paths(prefix, get_option('sbindir'))
matemandir = join_paths(prefix, get_option('mandir'))
pkgdatadir =  join_paths(matedatadir, meson.project_name())
mateicons = join_paths(pkgdatadir, 'icons')
mateui = join_paths(pkgdatadir, 'ui')
data_dir = join_paths(meson.current_source_dir(), 'data')
# get suported warning flags
test_args = [
  '-Waggregate-return',
  '-Warray-bounds',
  '-Wcast-align',
  '-Wclobbered',
  '-Wdeclaration-after-statement',
  '-Wempty-body',
  '-Wextra',
  '-Wformat=2',
  '-Wformat-nonliteral',
  '-Wformat-security',
  '-Wformat-signedness',
  '-Wignored-qualifiers',
  '-Wimplicit-function-declaration',
  '-Winit-self',
  '-Winline',
  '-Wmissing-declarations',
  '-Wmissing-format-attribute',
  '-Wmissing-include-dirs',
  '-Wmissing-noreturn',
  '-Wmissing-parameter-type',
  '-Wmissing-prototypes',
  '-Wnested-externs',
  '-Wno-discarded-qualifiers',
  '-Wno-missing-field-initializers',
  '-Wno-strict-aliasing',
  '-Wno-suggest-attribute=format',
  '-Wno-unused-parameter',
  '-Wold-style-definition',
  '-Woverride-init',
  '-Wpacked',
  '-Wpointer-arith',
  '-Wredundant-decls',
  '-Wreturn-type',
  '-Wshadow',
  '-Wsign-compare',
  '-Wstrict-aliasing',
  '-Wstrict-prototypes',
  '-Wswitch-default',
  '-Wtype-limits',
  '-Wundef',
  '-Wuninitialized',
  '-Wunused-but-set-variable',
  '-Wwrite-strings'
]
cc = meson.get_compiler('c')
foreach arg: test_args
  if cc.has_argument(arg)
    add_project_arguments(arg, language : 'c')
  endif
endforeach

common_flags = [
  '-DHAVE_CONFIG_H',
]

add_project_arguments(common_flags, language: 'c')
# enable full RELRO where possible
# FIXME: until https://github.com/mesonbuild/meson/issues/1140 is fixed
test_link_args = [
  '-Wl,-z,relro',
  '-Wl,-z,now',
]
foreach arg: test_link_args
  if cc.has_link_argument(arg)
    add_project_link_arguments(arg, language : 'c')
  endif
endforeach

enable_applet = get_option('applets')
enable_applets_inprocess = get_option('applets-in-process')
enable_keyring = get_option('gnome-keyring')
enable_libsecret = get_option('libsecret')

gtk = dependency('gtk+-3.0', version : '>= 3.22.0')
glib = dependency('glib-2.0', version : '>= 2.50.0')
gdk = dependency('gdk-3.0', version : '>= 3.22.0')
cairo = dependency('cairo', version : '>= 1.0.0')
xrandr = dependency('xrandr', version : '>= 1.3.0')
xproto = dependency('xproto', version : '>= 7.0.15')
x11 = dependency('x11', version : '>= 1.3.0')
xext = dependency('xext', version : '>= 1.3.0')
dbus = dependency('dbus-1', version : '>= 1.0')
dbusglib = dependency('dbus-glib-1', version : '>= 0.70')
notify = dependency('libnotify', version : '>= 0.7.0')
canberra = dependency('libcanberra-gtk3', version : '>= 0.10')
matepanel = dependency('libmatepanelapplet-4.0', version : '>= 1.17.0',required: enable_applet)
upower = dependency('upower-glib', version : '>= 0.99.8')
libsecret = dependency('libsecret-1', version : '>= 0.11', required: enable_libsecret)
keyring = dependency('gnome-keyring-1', version : '>= 3.0.0', required: enable_keyring)
canberra = dependency('libcanberra-gtk3', version : '>= 0.10')
md = dependency('mate-desktop-2.0', version : '>= 1.27.1')
libm = cc.find_library('m', required: false)

gnome = import('gnome')
i18n = import('i18n')

conf.set('WITH_KEYRING', keyring.found())
conf.set('WITH_LIBSECRET', libsecret.found())
conf.set('APPLETS_INPROCESS', enable_applets_inprocess)
conf.set_quoted('GETTEXT_PACKAGE', meson.project_name())
conf.set_quoted('LOCALEDIR',
                join_paths(get_option('prefix'),
                           get_option('localedir')))
conf.set_quoted('DATADIR',
                join_paths(get_option('prefix'),
                           get_option('datadir')))

conf.set_quoted('PACKAGE_URL', 'https://mate-desktop.org')
conf.set_quoted('VERSION', meson.project_version())

configure_file(
  output : 'config.h',
  configuration : conf
)
config_inc = include_directories('.')

if enable_applet
  subdir('applets/brightness')
  subdir('applets/inhibit')
  subdir('applets/power-profiles')
endif
subdir('help')
subdir('src')
subdir('policy')
subdir('po')
subdir('data')

# FIXME: remove when https://github.com/mesonbuild/meson/issues/837 fixed
meson.add_install_script('meson_post_install.sh')
