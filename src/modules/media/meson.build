media_sources = files('media_module.c', 'media_impl.c')

media_lib = static_library('helvetia_media',
  media_sources,
  dependencies        : [glib_dep, gtk_dep],
  include_directories : include_directories('..', '../..')
)

modules_libs += media_lib
