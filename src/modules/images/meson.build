images_sources = files('images_module.c', 'images_impl.c')

images_lib = static_library('helvetia_images',
  images_sources,
  dependencies        : [glib_dep, gtk_dep],
  include_directories : include_directories('..', '../..')
)

modules_libs += images_lib
