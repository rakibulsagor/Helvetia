convert_sources = files('convert_module.c', 'convert_impl.c')

convert_lib = static_library('helvetia_convert',
  convert_sources,
  dependencies        : [glib_dep, gtk_dep],
  include_directories : include_directories('..', '../..')
)

modules_libs += convert_lib
