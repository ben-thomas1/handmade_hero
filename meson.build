project('handmade_hero', 'cpp', 'c',
  meson_version : '>=1.3.0',
  default_options : [
    'cpp_std=c++26',
    'warning_level=2',
    'werror=false',
    'buildtype=debug',
  ],
)

cpp_args = [
  '-fno-exceptions',
  '-fno-rtti',
]
link_args = []
# Position-independent executable flags must stay off the shared game library.
exe_cpp_args = []
exe_link_args = []
target_options = []

if get_option('buildtype') == 'debug'
  cpp_args += [
    '-O0', # -Og breaks raddebugger for some reason
    '-g3',
    '-gdwarf-5',
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
  ]
  exe_cpp_args += [
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
    exe_link_args += [
      '-pie',
    ]
    link_args += [
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

if host_machine.system() == 'linux'
  wayland_scanner = find_program('wayland-scanner')
  xdg_shell_xml = dependency('wayland-protocols').get_variable(pkgconfig : 'pkgdatadir') / 'stable/xdg-shell/xdg-shell.xml'
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
  sources = files('src/handmade.cpp', 'src/linux/main.cpp', 'src/linux/wayland.cpp', 'src/linux/pipewire.cpp')
  sources += [xdg_shell_header, xdg_shell_code]
  deps = [
    dependency('wayland-client', include_type : 'system'),
    dependency('libpipewire-0.3', include_type : 'system'),
  ]
elif host_machine.system() == 'darwin'
  add_languages('objcpp', native : false)
  target_options += [
    'objcpp_std=c++26',
  ]
  sources = files('src/handmade.cpp', 'src/macos/main.mm', 'src/macos/cocoa.mm', 'src/macos/coreaudio.cpp')
  deps = [dependency('appleframeworks', modules : ['Cocoa', 'QuartzCore', 'AudioToolbox'])]
else
  error('Unsupported platform: ' + host_machine.system())
endif

executable('handmade_hero', sources,
  include_directories : include_directories('src'),
  dependencies : deps,
  cpp_args : cpp_args,
  objcpp_args : cpp_args,
  link_args : link_args,
  override_options : target_options,
  install : true,
)
