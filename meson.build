project('nativesigscan',
  'cpp',
  version : '0.1.0',
  default_options : ['cpp_std=c++17', 'warning_level=2'],
  meson_version : '>= 1.2.0',
)

# get opts
apicommit_opt = get_option('bn_api_ver')
# bn_headless_opt = get_option('bn_headless')

# configure bn api
bn_api_wrap_config_data = configuration_data()
bn_api_wrap_config_data.set('APICOMMIT', apicommit_opt)

# find a Python 3 interpreter
python3 = find_program('python3', required : true)

# generate the actual 'binaryninja-api.wrap' using script
# this is a hack to avoid hardcoding the commit in the wrap file
# it would be nice if meson wasn't so stubborn!! https://github.com/mesonbuild/meson/issues/2320
run_command(
  python3,
  meson.current_source_dir() / 'generate_wrap.py',
  meson.current_source_dir() / 'subprojects' / 'binaryninja-api.wrap',
  meson.project_build_root() / 'subprojects' / 'binaryninja-api.wrap',
  '@APICOMMIT@',
  apicommit_opt,
  check : true
)
message('Generated binaryninja-api.wrap for API commit: ' + apicommit_opt)

# bn_api_subproject_options = {'headless': bn_headless_opt.to_string()}
bn_api_subproject_options = {}
bn_api_subproject = subproject('binaryninja-api',
  default_options : bn_api_subproject_options,
  required : true
)

binaryninja_api_dep = dependency('binaryninjaapi')

subdir('src')