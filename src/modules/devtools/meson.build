devtools_sources = files('devtools_module.c', 'devtools_impl.c')

devtools_lib = static_library('helvetia_devtools',
  devtools_sources,
  dependencies        : [glib_dep, gtk_dep],
  include_directories : include_directories('..', '../..')
)

modules_libs += devtools_lib
