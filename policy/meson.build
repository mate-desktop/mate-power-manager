po_dir = join_paths(meson.source_root(), 'po')
dataconf = configuration_data()
dataconf.set('sbindir', matesbindir)

i18n.merge_file(
  input : configure_file(
  configuration: dataconf,
  input : 'org.mate.power.policy.in2',
  output: 'org.mate.power.policy.in'
  ),
  output: 'org.mate.power.policy',
  po_dir: po_dir,
  install: true,
  install_dir: join_paths(get_option('datadir'), 'polkit-1', 'actions'),
)
