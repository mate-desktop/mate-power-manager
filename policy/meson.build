backlight_policy_data = configuration_data()
backlight_policy_data.set('sbindir', sbindir_path)

backlight_policy = configure_file(
  input: 'org.mate.power.policy.in2',
  output: 'org.mate.power.policy.in',
  configuration: backlight_policy_data
)

i18n.merge_file(
  input: backlight_policy,
  output: 'org.mate.power.policy',
  type: 'xml',
  po_dir: podir,
  install: true,
  install_dir: join_paths(datadir_path, 'polkit-1', 'actions')
)
