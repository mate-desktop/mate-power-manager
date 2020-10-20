actioniconsdir_path = join_paths(datadir_path, 'icons', 'hicolor')
appiconsdir_path = join_paths(datadir_path, 'icons', 'hicolor')
statusiconsdir_path = join_paths(datadir_path, 'icons', 'hicolor')

action_images = [
  'gpm-hibernate.png',
  'gpm-suspend.png'
]

app_images = [
  'mate-brightness-applet.png',
  'mate-inhibit-applet.png',
  'mate-power-manager.png',
  'mate-power-statistics.png'
] 

status_images = [
  'gpm-ac-adapter.png', 'gpm-keyboard-080.png', 'gpm-battery-000-charging.png',
  'gpm-keyboard-100.png', 'gpm-battery-000.png', 'gpm-mouse-000.png',
  'gpm-battery-020-charging.png', 'gpm-mouse-020.png', 'gpm-battery-020.png',
  'gpm-mouse-040.png', 'gpm-battery-040-charging.png', 'gpm-mouse-060.png',
  'gpm-battery-040.png', 'gpm-mouse-080.png', 'gpm-battery-060-charging.png',
  'gpm-mouse-100.png', 'gpm-battery-060.png', 'gpm-phone-000.png',
  'gpm-battery-080-charging.png', 'gpm-phone-020.png', 'gpm-battery-080.png',
  'gpm-phone-040.png', 'gpm-battery-100-charging.png', 'gpm-phone-060.png',
  'gpm-battery-100.png', 'gpm-phone-080.png', 'gpm-battery-charged.png',
  'gpm-phone-100.png', 'gpm-battery-empty.png', 'gpm-ups-000-charging.png',
  'gpm-battery-missing.png', 'gpm-ups-000.png', 'gpm-brightness-kbd-disabled.png',
  'gpm-ups-020-charging.png','gpm-brightness-kbd-invalid.png', 'gpm-ups-020.png',
  'gpm-brightness-kbd.png', 'gpm-ups-040-charging.png', 'gpm-brightness-lcd-disabled.png',
  'gpm-ups-040.png', 'gpm-brightness-lcd-invalid.png', 'gpm-ups-060-charging.png',
  'gpm-brightness-lcd.png', 'gpm-ups-060.png', 'gpm-inhibit-invalid.png', 'gpm-ups-080-charging.png',
  'gpm-inhibit.png', 'gpm-ups-080.png', 'gpm-keyboard-000.png', 'gpm-ups-100-charging.png',
  'gpm-keyboard-020.png', 'gpm-ups-100.png', 'gpm-keyboard-040.png', 'gpm-ups-missing.png',
  'gpm-keyboard-060.png'
]

foreach s: ['16x16', '22x22', '24x24', '32x32', '48x48', '64x64', '96x96', '128x128', '256x256']
  foreach image: action_images
    install_data(
      join_paths(s, 'actions', image),
      install_dir: join_paths(actioniconsdir_path, s, 'actions')
    )
    endforeach

  foreach image: app_images
    install_data(
      join_paths(s, 'apps', image),
      install_dir: join_paths(appiconsdir_path, s, 'apps')
    )
  endforeach

  foreach image: status_images
    install_data(
      join_paths(s, 'status', image),
      install_dir: join_paths(statusiconsdir_path, s, 'status')
    )
  endforeach
endforeach
