project('handmade_hero', 'cpp',
  meson_version : '>=1.3.0',
  default_options : ['cpp_std=c++26', 'warning_level=2', 'buildtype=debug'],
)

cpp_args = ['-fno-exceptions', '-fno-rtti']
link_args = []
target_options = []

if get_option('buildtype') == 'debug'
  cpp_args += [
    # Keep optimization disabled for predictable source stepping.
    '-O0', '-g3', '-gdwarf-5',
    '-Wshadow', '-Wconversion', '-Wsign-conversion', '-Wvla',
    '-Wcast-align', '-Wpointer-arith', '-Wwrite-strings', '-Wformat=2',
    '-Wnull-dereference', '-Wdouble-promotion', '-Wmissing-declarations',
    '-Wimplicit-fallthrough', '-fno-sanitize-recover=all',
    '-fno-omit-frame-pointer', '-fsanitize-address-use-after-scope',
    '-ftrivial-auto-var-init=pattern', '-fstack-protector-all',
  ]
elif get_option('buildtype') == 'release'
  cpp_args += [
    '-march=native', '-mtune=native', '-ffast-math',
    '-fomit-frame-pointer', '-fno-stack-protector', '-fPIE',
  ]
  target_options += ['b_lto=true', 'b_ndebug=true']

  if host_machine.system() == 'linux'
    cpp_args += ['-fcf-protection=full']
    link_args += ['-pie', '-Wl,-z,relro,-z,now', '-Wl,-z,noexecstack']
  elif host_machine.system() == 'darwin' and host_machine.cpu_family() == 'aarch64'
    cpp_args += ['-mbranch-protection=standard']
  endif
endif

# Clang's LTO works with lld without a separate LLVMgold plugin.
if host_machine.system() == 'linux' and meson.get_compiler('cpp').get_id() == 'clang'
  link_args += ['-fuse-ld=lld']
endif

executable('handmade_hero', 'main.cpp',
  cpp_args : cpp_args,
  link_args : link_args,
  override_options : target_options,
  install : true,
)
