project('nativesigscan',
  'cpp',
  version : '0.1.0',
  default_options : ['cpp_std=c++17', 'warning_level=2'],
  meson_version : '>= 1.2.0',
)

bn_api_subproject_options = {}
bn_api_subproject = subproject('binaryninja-api',
  default_options : bn_api_subproject_options,
  required : true
)

binaryninja_api_dep = dependency('binaryninjaapi')

subdir('src')