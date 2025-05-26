project('nativesigscan',
  'cpp',
  version : '0.1.0',
  default_options : ['cpp_std=c++17', 'warning_level=2'],
  meson_version : '>= 1.2.0',
)

# configure binaryninja api cmake project
# https://mesonbuild.com/CMake-module.html
cmake = import('cmake')

bn_headless_cmake_opt = cmake.subproject_options()

message('Configuring binaryninja-api subproject using CMake module...')
bnapi_subproj = cmake.subproject(
  'binaryninja-api',
  options : bn_headless_cmake_opt,
)
# message('CMake targets:\n - ' + '\n - '.join(bnapi_subproj.target_list()))

binaryninja_api_dep = bnapi_subproj.dependency('binaryninjaapi')
binaryninja_fmt_dep = bnapi_subproj.dependency('fmt')
binaryninja_ui_dep = bnapi_subproj.dependency('binaryninjaui')

binaryninja_lib_deps = [
  binaryninja_api_dep,
  binaryninja_fmt_dep,
  binaryninja_ui_dep,
]

message('Binaryninja API (CMake) dependency obtained.')

# define plugin library
plugin_target_name = meson.project_name()

plugin_sources = [
  './src/sigscan.cpp',
]

plugin_cpp_args = [
]

# build plugin as shared library
shared_library(plugin_target_name,
  plugin_sources,
  dependencies : binaryninja_lib_deps,
  cpp_args : plugin_cpp_args,
  name_prefix : '',
)
