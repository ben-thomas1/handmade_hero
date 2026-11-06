add_languages('objcpp', native : false)
target_options += [
  'objcpp_std=c++26',
]

frameworks = dependency('appleframeworks', modules : [
  'Cocoa', 'QuartzCore', 'IOSurface', 'CoreGraphics', 'AudioToolbox',
])

platform_sources = files('main.mm', 'cocoa.mm', 'coreaudio.cpp')
platform_deps = [frameworks]
platform_inc = []
