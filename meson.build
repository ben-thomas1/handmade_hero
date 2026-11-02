project('handmade_hero', 'cpp', 'c',
  meson_version : '>=1.3.0',
  default_options : [
    'cpp_std=c++26',
    'warning_level=2',
    'buildtype=debug',
  ],
)

cpp_args = [
  '-fno-exceptions',
  '-fno-rtti',
]
link_args = []
target_options = []

if get_option('buildtype') == 'debug'
  cpp_args += [
    '-O0', # -Og breaks raddebugger for some reason
    '-g3',
    '-gdwarf-5',
    '-Wshadow',
    '-Wconversion',
    '-Wsign-conversion',
    '-Wvla',
    '-Wcast-align',
    '-Wpointer-arith',
    '-Wwrite-strings',
    '-Wformat=2',
    '-Wnull-dereference',
    '-Wdouble-promotion',
    '-Wmissing-declarations',
    '-Wimplicit-fallthrough',
    '-fno-sanitize-recover=all',
    '-fno-omit-frame-pointer',
    '-fsanitize-address-use-after-scope',
    '-ftrivial-auto-var-init=pattern',
    '-fstack-protector-all',
  ]
elif get_option('buildtype') == 'release'
  cpp_args += [
    '-march=native',
    '-mtune=native',
    '-ffast-math',
    '-fomit-frame-pointer',
    '-fno-stack-protector',
    '-fPIE',
  ]
  target_options += [
    'b_lto=true',
    'b_ndebug=true',
  ]

  if host_machine.system() == 'linux'
    cpp_args += [
      '-fcf-protection=full',
    ]
    link_args += [
      '-pie',
      '-Wl,-z,relro,-z,now',
      '-Wl,-z,noexecstack',
    ]
  elif host_machine.system() == 'darwin' and host_machine.cpu_family() == 'aarch64'
    cpp_args += [
      '-mbranch-protection=standard',
    ]
  endif
endif

# Clang's LTO works with lld without a separate LLVMgold plugin.
if host_machine.system() == 'linux' and meson.get_compiler('cpp').get_id() == 'clang'
  link_args += [
    '-fuse-ld=lld',
  ]
endif

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

executable('handmade_hero', [
  'src/main.cpp', 'src/window.cpp',
  xdg_shell_header, xdg_shell_code,
  viewporter_header, viewporter_code,
  xdg_decoration_header, xdg_decoration_code,
],
  dependencies : [wayland],
  cpp_args : cpp_args,
  link_args : link_args,
  override_options : target_options,
  install : true,
)
