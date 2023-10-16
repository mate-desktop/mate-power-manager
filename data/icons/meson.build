apps_icons = [
  ['16x16', 'mate-brightness-applet.png', 'mate-inhibit-applet.png', 'mate-power-manager.png', 'mate-power-statistics.png'],
  ['22x22', 'mate-brightness-applet.png', 'mate-inhibit-applet.png', 'mate-power-manager.png', 'mate-power-statistics.png'],
  ['24x24', 'mate-brightness-applet.png', 'mate-inhibit-applet.png', 'mate-power-manager.png', 'mate-power-statistics.png'],
  ['32x32', 'mate-brightness-applet.png', 'mate-inhibit-applet.png', 'mate-power-manager.png', 'mate-power-statistics.png'],
  ['48x48', 'mate-brightness-applet.png', 'mate-inhibit-applet.png', 'mate-power-manager.png', 'mate-power-statistics.png'],
  ['64x64', 'mate-brightness-applet.png', 'mate-inhibit-applet.png', 'mate-power-manager.png', 'mate-power-statistics.png'],
  ['96x96', 'mate-brightness-applet.png', 'mate-inhibit-applet.png', 'mate-power-manager.png', 'mate-power-statistics.png'],
  ['128x128', 'mate-brightness-applet.png', 'mate-inhibit-applet.png', 'mate-power-manager.png', 'mate-power-statistics.png'],
  ['256x256', 'mate-brightness-applet.png', 'mate-inhibit-applet.png', 'mate-power-manager.png', 'mate-power-statistics.png'],
]
actions_icons = [
  ['16x16', 'gpm-hibernate.png', 'gpm-suspend.png'],
  ['22x22', 'gpm-hibernate.png', 'gpm-suspend.png'],
  ['24x24', 'gpm-hibernate.png', 'gpm-suspend.png'],
  ['32x32', 'gpm-hibernate.png', 'gpm-suspend.png'],
  ['48x48', 'gpm-hibernate.png', 'gpm-suspend.png'],
  ['64x64', 'gpm-hibernate.png', 'gpm-suspend.png'],
  ['96x96', 'gpm-hibernate.png', 'gpm-suspend.png'],
  ['128x128', 'gpm-hibernate.png', 'gpm-suspend.png'],
  ['256x256', 'gpm-hibernate.png', 'gpm-suspend.png'],
]

icons = [
  'gpm-ac-adapter.png',
  'gpm-battery-000-charging.png',
  'gpm-battery-000.png',
  'gpm-battery-020-charging.png',
  'gpm-battery-020.png',
  'gpm-battery-040-charging.png',
  'gpm-battery-040.png',
  'gpm-battery-060-charging.png',
  'gpm-battery-060.png',
  'gpm-battery-080-charging.png',
  'gpm-battery-080.png',
  'gpm-battery-100-charging.png',
  'gpm-battery-100.png',
  'gpm-battery-charged.png',
  'gpm-battery-empty.png',
  'gpm-battery-missing.png',
  'gpm-brightness-kbd-disabled.png',
  'gpm-brightness-kbd-invalid.png',
  'gpm-brightness-kbd.png',
  'gpm-brightness-lcd-disabled.png',
  'gpm-brightness-lcd-invalid.png',
  'gpm-brightness-lcd.png',
  'gpm-gaming-input-000.png',
  'gpm-gaming-input-020.png',
  'gpm-gaming-input-040.png',
  'gpm-gaming-input-060.png',
  'gpm-gaming-input-080.png',
  'gpm-gaming-input-100.png',
  'gpm-inhibit-invalid.png',
  'gpm-inhibit.png',
  'gpm-keyboard-000.png',
  'gpm-keyboard-020.png',
  'gpm-keyboard-040.png',
  'gpm-keyboard-060.png',
  'gpm-keyboard-080.png',
  'gpm-keyboard-100.png',
  'gpm-mouse-000.png',
  'gpm-mouse-020.png',
  'gpm-mouse-040.png',
  'gpm-mouse-060.png',
  'gpm-mouse-080.png',
  'gpm-mouse-100.png',
  'gpm-phone-000.png',
  'gpm-phone-020.png',
  'gpm-phone-040.png',
  'gpm-phone-060.png',
  'gpm-phone-080.png',
  'gpm-phone-100.png',
  'gpm-ups-000-charging.png',
  'gpm-ups-000.png',
  'gpm-ups-020-charging.png',
  'gpm-ups-020.png',
  'gpm-ups-040-charging.png',
  'gpm-ups-040.png',
  'gpm-ups-060-charging.png',
  'gpm-ups-060.png',
  'gpm-ups-080-charging.png',
  'gpm-ups-080.png',
  'gpm-ups-100-charging.png',
  'gpm-ups-100.png',
  'gpm-ups-missing.png',
]

pixels = [
  '16x16',
  '22x22',
  '24x24',
  '32x32',
  '48x48',
  '64x64',
  '96x96',
  '128x128',
  '256x256',
]
pixels2 = [
  '48x48',
  '64x64',
  '96x96',
  '128x128',
  '256x256',
]
foreach pixel: pixels

  foreach icon: icons
    install_data(
      join_paths (pixel, 'status', icon),
      install_dir : join_paths(pkgdatadir, 'icons/hicolor', pixel, 'status'))
  endforeach

endforeach

foreach pixel: pixels2
  install_data(
      join_paths (pixel, 'status', 'gpm-monitor.png'),
      install_dir : join_paths(pkgdatadir, 'icons/hicolor', pixel, 'status'))
endforeach

foreach icon: actions_icons
  install_data(
    join_paths(icon[0], 'actions', icon[1]),
    install_dir: join_paths(pkgdatadir, 'icons/hicolor', icon[0], 'actions')
  )
  install_data(
    join_paths(icon[0], 'actions', icon[2]),
    install_dir: join_paths(pkgdatadir, 'icons/hicolor', icon[0], 'actions')
  )
endforeach

foreach icon: apps_icons
  install_data(
    join_paths(icon[0], 'apps', icon[1]),
    install_dir: join_paths(matedatadir, 'icons/hicolor', icon[0], 'apps')
  )
  install_data(
    join_paths(icon[0], 'apps', icon[2]),
    install_dir: join_paths(matedatadir, 'icons/hicolor', icon[0], 'apps')
  )
  install_data(
    join_paths(icon[0], 'apps', icon[3]),
    install_dir: join_paths(matedatadir, 'icons/hicolor', icon[0], 'apps')
  )
  install_data(
    join_paths(icon[0], 'apps', icon[4]),
    install_dir: join_paths(matedatadir, 'icons/hicolor', icon[0], 'apps')
  )
endforeach

install_subdir('scalable/actions', install_dir : join_paths(pkgdatadir, 'icons', 'hicolor', 'scalable'))
install_subdir('scalable/status', install_dir : join_paths(pkgdatadir, 'icons', 'hicolor', 'scalable'))
install_subdir('scalable/apps', install_dir : join_paths(matedatadir, 'icons', 'hicolor', 'scalable'))
