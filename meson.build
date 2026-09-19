project('helvetia', 'c',
  version : '0.1.0',
  license : 'MIT',
  default_options : [
    'c_std=c17',
    'warning_level=3',
    'werror=false',
    'buildtype=debugoptimized',
  ])

add_project_arguments('-Wno-pedantic', language : 'c')

# ---- Dependencies ----
gtk_dep     = dependency('gtk4',      version : '>=4.10')
glib_dep    = dependency('glib-2.0',  version : '>=2.74')
gmodule_dep = dependency('gmodule-2.0')
math_dep    = meson.get_compiler('c').find_library('m', required : false)
openssl_dep = dependency('openssl')
json_c_dep  = dependency('json-c')

# ---- Global config ----
conf = configuration_data()
conf.set_quoted('APP_ID',      'org.helvetia.Helvetia')
conf.set_quoted('APP_NAME',    'Helvetia')
conf.set_quoted('APP_VERSION', meson.project_version())
conf.set_quoted('PLUGIN_DIR',  join_paths(get_option('prefix'), get_option('libdir'), 'helvetia', 'plugins'))
configure_file(output : 'config.h', configuration : conf)

# Expose the build root as an include dir so generated config.h is reachable
config_inc = include_directories('.')

# ---- Sub-directories ----
subdir('src')

registry_test = executable('registry-test',
  'tests/test_registry.c',
  dependencies        : [glib_dep, gtk_dep, gmodule_dep],
  link_with           : core_lib,
  include_directories : [include_directories('src'), config_inc])
test('registry', registry_test)

quirc_test = executable('quirc-test',
  'tests/test_quirc.c',
  dependencies        : [glib_dep, gtk_dep, math_dep],
  link_with           : utility_lib,
  include_directories : [include_directories('src'), config_inc])
test('quirc', quirc_test)
