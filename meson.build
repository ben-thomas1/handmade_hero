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

inc = include_directories('src')

subdir('src/game')

# Debug builds load the game library at runtime and reload it when it is rebuilt.
if get_option('buildtype') == 'debug'
  exe_cpp_args += [
    '-DHOT_RELOAD=1',
    '-DGAME_LIBRARY_PATH="' + game.full_path() + '"',
  ]
  exe_link_with = []
  exe_deps = [dependency('dl')]
else
  exe_cpp_args += [
    '-DHOT_RELOAD=0',
  ]
  exe_link_with = [game]
  exe_deps = []
endif

if host_machine.system() == 'linux'
  subdir('src/platform/linux')
elif host_machine.system() == 'darwin'
  subdir('src/platform/macos')
else
  error('Unsupported platform: ' + host_machine.system())
endif

executable('handmade_hero', platform_sources,
  include_directories : [inc, platform_inc],
  link_with : exe_link_with,
  dependencies : platform_deps + exe_deps,
  cpp_args : cpp_args + exe_cpp_args,
  objcpp_args : cpp_args + exe_cpp_args,
  link_args : link_args + exe_link_args,
  override_options : target_options,
  install : true,
)
