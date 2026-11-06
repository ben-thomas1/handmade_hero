pipewire = dependency('libpipewire-0.3', include_type : 'system')
dbus = dependency('dbus-1', include_type : 'system')

wayland = dependency('wayland-client', include_type : 'system')
wayland_protocols = dependency('wayland-protocols')
wayland_scanner = find_program('wayland-scanner')
xdg_shell_xml = wayland_protocols.get_variable(pkgconfig : 'pkgdatadir') / 'stable/xdg-shell/xdg-shell.xml'

xdg_shell_header = custom_target('xdg-shell-header',
  input : xdg_shell_xml,
  output : 'xdg-shell-client-protocol.h',
  command : [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
)
xdg_shell_code = custom_target('xdg-shell-code',
  input : xdg_shell_xml,
  output : 'xdg-shell-protocol.c',
  command : [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
)

viewporter_xml = wayland_protocols.get_variable(pkgconfig : 'pkgdatadir') / 'stable/viewporter/viewporter.xml'
viewporter_header = custom_target('viewporter-header',
  input : viewporter_xml,
  output : 'viewporter-client-protocol.h',
  command : [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
)
viewporter_code = custom_target('viewporter-code',
  input : viewporter_xml,
  output : 'viewporter-protocol.c',
  command : [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
)

xdg_decoration_xml = wayland_protocols.get_variable(pkgconfig : 'pkgdatadir') / 'unstable/xdg-decoration/xdg-decoration-unstable-v1.xml'
xdg_decoration_header = custom_target('xdg-decoration-header',
  input : xdg_decoration_xml,
  output : 'xdg-decoration-client-protocol.h',
  command : [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
)
xdg_decoration_code = custom_target('xdg-decoration-code',
  input : xdg_decoration_xml,
  output : 'xdg-decoration-protocol.c',
  command : [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
)

platform_sources = files('main.cpp', 'wayland.cpp', 'pipewire.cpp') + [
  xdg_shell_header, xdg_shell_code,
  viewporter_header, viewporter_code,
  xdg_decoration_header, xdg_decoration_code,
]
platform_deps = [wayland, pipewire, dbus]
# Covers the build dir too, where wayland-scanner writes its headers.
platform_inc = include_directories('.')
