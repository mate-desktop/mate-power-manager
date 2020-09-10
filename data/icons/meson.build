actioniconsdir_path = join_paths(pkgdatadir_path, 'icons', 'hicolor')
appiconsdir_path = join_paths(datadir_path, 'icons', 'hicolor')
statusiconsdir_path = join_paths(pkgdatadir_path, 'icons', 'hicolor')

foreach s: ['16x16', '22x22', '24x24', '32x32', '48x48', '64x64', '96x96', '128x128', '256x256']
  install_data(
    [
      join_paths(s, 'actions', 'gpm-hibernate.png'),
      join_paths(s, 'actions', 'gpm-suspend.png'),
    ],
    install_dir: join_paths(actioniconsdir_path, s, 'actions')
  )

  install_data(
    [
      join_paths(s, 'apps', 'mate-brightness-applet.png'),
      join_paths(s, 'apps', 'mate-inhibit-applet.png'),
      join_paths(s, 'apps', 'mate-power-manager.png'),
      join_paths(s, 'apps', 'mate-power-statistics.png'),
    ],
    install_dir: join_paths(appiconsdir_path, s, 'apps')
  )
  install_data(
    [
      join_paths(s, 'status', 'gpm-ac-adapter.png'), join_paths(s, 'status', 'gpm-keyboard-080.png'),
      join_paths(s, 'status', 'gpm-battery-000-charging.png'), join_paths(s, 'status', 'gpm-keyboard-100.png'),
      join_paths(s, 'status', 'gpm-battery-000.png'), join_paths(s, 'status', 'gpm-mouse-000.png'),
      join_paths(s, 'status', 'gpm-battery-020-charging.png'), join_paths(s, 'status', 'gpm-mouse-020.png'),
      join_paths(s, 'status', 'gpm-battery-020.png'), join_paths(s, 'status', 'gpm-mouse-040.png'),
      join_paths(s, 'status', 'gpm-battery-040-charging.png'), join_paths(s, 'status', 'gpm-mouse-060.png'),
      join_paths(s, 'status', 'gpm-battery-040.png'), join_paths(s, 'status', 'gpm-mouse-080.png'),
      join_paths(s, 'status', 'gpm-battery-060-charging.png'), join_paths(s, 'status', 'gpm-mouse-100.png'),
      join_paths(s, 'status', 'gpm-battery-060.png'), join_paths(s, 'status', 'gpm-phone-000.png'),
      join_paths(s, 'status', 'gpm-battery-080-charging.png'), join_paths(s, 'status', 'gpm-phone-020.png'),
      join_paths(s, 'status', 'gpm-battery-080.png'), join_paths(s, 'status', 'gpm-phone-040.png'),
      join_paths(s, 'status', 'gpm-battery-100-charging.png'), join_paths(s, 'status', 'gpm-phone-060.png'),
      join_paths(s, 'status', 'gpm-battery-100.png'), join_paths(s, 'status', 'gpm-phone-080.png'),
      join_paths(s, 'status', 'gpm-battery-charged.png'), join_paths(s, 'status', 'gpm-phone-100.png'),
      join_paths(s, 'status', 'gpm-battery-empty.png'), join_paths(s, 'status', 'gpm-ups-000-charging.png'),
      join_paths(s, 'status', 'gpm-battery-missing.png'), join_paths(s, 'status', 'gpm-ups-000.png'),
      join_paths(s, 'status', 'gpm-brightness-kbd-disabled.png'), join_paths(s, 'status', 'gpm-ups-020-charging.png'),
      join_paths(s, 'status', 'gpm-brightness-kbd-invalid.png'), join_paths(s, 'status', 'gpm-ups-020.png'),
      join_paths(s, 'status', 'gpm-brightness-kbd.png'), join_paths(s, 'status', 'gpm-ups-040-charging.png'),
      join_paths(s, 'status', 'gpm-brightness-lcd-disabled.png'), join_paths(s, 'status', 'gpm-ups-040.png'),
      join_paths(s, 'status', 'gpm-brightness-lcd-invalid.png'), join_paths(s, 'status', 'gpm-ups-060-charging.png'),
      join_paths(s, 'status', 'gpm-brightness-lcd.png'), join_paths(s, 'status', 'gpm-ups-060.png'),
      join_paths(s, 'status', 'gpm-inhibit-invalid.png'), join_paths(s, 'status', 'gpm-ups-080-charging.png'),
      join_paths(s, 'status', 'gpm-inhibit.png'), join_paths(s, 'status', 'gpm-ups-080.png'),
      join_paths(s, 'status', 'gpm-keyboard-000.png'), join_paths(s, 'status', 'gpm-ups-100-charging.png'),
      join_paths(s, 'status', 'gpm-keyboard-020.png'), join_paths(s, 'status', 'gpm-ups-100.png'),
      join_paths(s, 'status', 'gpm-keyboard-040.png'), join_paths(s, 'status', 'gpm-ups-missing.png'),
      join_paths(s, 'status', 'gpm-keyboard-060.png'),
    ],
    install_dir: join_paths(statusiconsdir_path, s, 'status')
  )
endforeach
