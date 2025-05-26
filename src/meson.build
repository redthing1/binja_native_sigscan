plugin_target_name = meson.project_name()

plugin_sources = [
  'sigscan.cpp',
]

plugin_cpp_args = [
]

# build plugin as shared library
shared_library(plugin_target_name,
  plugin_sources,
  dependencies : binaryninja_lib_deps,
  cpp_args : plugin_cpp_args,
  name_prefix : '', # don't add 'lib' prefix to the output library name
)
