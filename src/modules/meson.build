modules_libs = []

subdir('pdf')
subdir('image')
modules_libs += helvetia_image_lib
subdir('security')

modules_lib = static_library('helvetia_modules',
  'builtin.c',
  dependencies        : [glib_dep, gtk_dep, adw_dep],
  include_directories : include_directories('..'),
  link_with           : modules_libs,
)
