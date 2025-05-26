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

binaryninja_api_dep = bnapi_subproj.dependency('binaryninjaapi')
message('Binaryninja API (CMake) dependency obtained.')

subdir('src')