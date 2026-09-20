image_sources = files(
  'image_module.c',
  'image_shared.c',
  'tools/image_viewer.c',
  'tools/image_slideshow.c',
  'tools/image_thumbnail_grid.c',
  'tools/image_exif_viewer.c',
  'tools/image_metadata_inspector.c',
)

helvetia_image_lib = static_library('helvetia_image',
  image_sources,
  dependencies : [glib_dep, gtk_dep, adw_dep, exif_dep],
  include_directories : include_directories('.', '../..'),
  pic : true,
)
