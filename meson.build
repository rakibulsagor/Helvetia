project('helvetia', ['c', 'cpp'],
  version : '0.1.0',
  license : 'MIT',
  default_options : [
    'c_std=c17',
    'warning_level=3',
    'werror=false',
    'buildtype=debugoptimized',
  ])

add_project_arguments('-Wno-pedantic', language : 'c')

# ------------------------------------------------------------------ #
# Sanitizer flags                                                    #
# ------------------------------------------------------------------ #

cc  = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')

sanitize_choice = get_option('sanitize')
sanitize_flags  = []

if sanitize_choice != 'none'
  # ASan, UBSan, TSan — mutually exclusive except address+undefined
  if sanitize_choice == 'address,undefined'
    sanitize_flags = ['-fsanitize=address,undefined',
                      '-fno-omit-frame-pointer',
                      '-fno-common']
  else
    sanitize_flags = ['-fsanitize=' + sanitize_choice,
                      '-fno-omit-frame-pointer',
                      '-fno-common']
  endif

  # TSan needs special handling: gtk_init() is not thread-safe under TSan
  if sanitize_choice == 'thread'
    sanitize_flags += ['-fPIE', '-pie']
  endif

  if not cc.has_multi_arguments(sanitize_flags)
    error('Compiler does not support -fsanitize=' + sanitize_choice)
  endif
endif

# Coverage
if get_option('coverage')
  sanitize_flags += ['--coverage', '-fprofile-arcs', '-ftest-coverage']
endif

add_project_arguments(sanitize_flags, language : ['c', 'cpp'])
add_project_link_arguments(sanitize_flags, language : ['c', 'cpp'])

# ---- Dependencies ----
gtk_dep     = dependency('gtk4',      version : '>= 4.16')
adw_dep     = dependency('libadwaita-1', version : '>= 1.6')
glib_dep    = dependency('glib-2.0',  version : '>=2.74')
gio_dep     = dependency('gio-2.0')
gobject_dep = dependency('gobject-2.0')
gmodule_dep = dependency('gmodule-2.0')
math_dep    = meson.get_compiler('c').find_library('m', required : false)
openssl_dep = dependency('openssl')
json_c_dep  = dependency('json-c')
libsodium_dep = dependency('libsodium')
libarchive_dep = dependency('libarchive')
gegl_dep   = dependency('gegl-0.4',  version : '>= 0.4.30')
babl_dep   = dependency('babl-0.1',  version : '>= 0.1.80')
libraw_dep = dependency('libraw',    version : '>= 0.20')
exif_dep   = dependency('libexif')

avcodec_dep    = dependency('libavcodec',    version : '>= 59', required : false)
avformat_dep   = dependency('libavformat',   version : '>= 59', required : false)
avutil_dep     = dependency('libavutil',     version : '>= 57', required : false)
avfilter_dep   = dependency('libavfilter',   version : '>= 8',  required : false)
swscale_dep    = dependency('libswscale',    version : '>= 6',  required : false)
swresample_dep = dependency('libswresample', version : '>= 4',  required : false)

# ---- Global config ----
conf = configuration_data()
conf.set_quoted('APP_ID',      'org.helvetia.Helvetia')
conf.set_quoted('APP_NAME',    'Helvetia')
conf.set_quoted('APP_VERSION', meson.project_version())
conf.set_quoted('PLUGIN_DIR',  join_paths(get_option('prefix'), get_option('libdir'), 'helvetia', 'plugins'))
configure_file(output : 'config.h', configuration : conf)

# Expose the build root as an include dir so generated config.h is reachable
config_inc = include_directories('.')

# ------------------------------------------------------------------ #
# Test environment                                                   #
# ------------------------------------------------------------------ #

test_env = environment()
test_env.set('G_DEBUG', 'fatal-warnings,fatal-criticals')
test_env.set('G_SLICE', 'always-malloc')
test_env.set('MALLOC_CHECK_', '3')

# ASan options when the app is built with -Dsanitize=address
test_env.set('ASAN_OPTIONS',
  'detect_leaks=1:' +
  'abort_on_error=1:' +
  'strict_string_checks=1:' +
  'detect_stack_use_after_return=1:' +
  'check_initialization_order=1:' +
  'print_stacktrace=1:' +
  'halt_on_error=1')

test_env.set('UBSAN_OPTIONS',
  'print_stacktrace=1:' +
  'halt_on_error=1:' +
  'abort_on_error=1')

valgrind = find_program('valgrind', required : get_option('valgrind'))
valgrind_suppressions = files('tests/valgrind.supp')

valgrind_env = environment()
valgrind_env.set('G_SLICE', 'always-malloc')
valgrind_env.set('G_DEBUG', 'gc-friendly')

valgrind_args = [
  '--tool=memcheck',
  '--leak-check=full',
  '--show-leak-kinds=definite,indirect,possible',
  '--errors-for-leak-kinds=definite,indirect',
  '--track-origins=yes',
  '--error-exitcode=42',
  '--num-callers=30',
  '--gen-suppressions=all',
  '--suppressions=' + valgrind_suppressions[0].full_path(),
]

# ---- Sub-directories ----
subdir('src')

registry_test = executable('registry-test',
  'tests/test_registry.c',
  dependencies        : [glib_dep, gtk_dep, gmodule_dep],
  link_with           : core_lib,
  include_directories : [include_directories('src'), config_inc])

test('registry', registry_test, env: test_env, suite: 'core')
if get_option('valgrind') and valgrind.found()
  test('registry-valgrind', valgrind, args: valgrind_args + [registry_test], env: valgrind_env, timeout: 300, suite: ['core', 'valgrind'])
endif

# Accent color tests
accent_test = executable('accent-test',
  'tests/test_accent.c',
  dependencies        : [glib_dep, gtk_dep, adw_dep],
  include_directories : [config_inc])

accents = ['blue', 'red', 'green', 'yellow', 'orange', 'purple', 'slate', 'pink', 'teal']
foreach acc : accents
  acc_env = environment()
  acc_env.set('ADW_ACCENT_COLOR', acc)
  test('accent-' + acc, accent_test, env: acc_env, suite: 'ui')
endforeach




# ------------------------------------------------------------------ #
# Aggregate target: run all tests under Valgrind                     #
# ------------------------------------------------------------------ #

if get_option('valgrind') and valgrind.found()
  run_target('test-valgrind',
    command : [find_program('tests/run_valgrind_all.sh'),
               meson.project_build_root()])
endif

# Aggregate target: build with ASan and run tests
if get_option('sanitize') != 'none'
  run_target('test-sanitized',
    command : [find_program('tests/run_sanitized_tests.sh'),
               meson.project_build_root()])
endif
