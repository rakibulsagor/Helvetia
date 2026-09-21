image_sources = files(
  'image_module.c',
  'image_shared.c',
  'tools/image_viewer.c',
  'tools/image_slideshow.c',
  'tools/image_thumbnail_grid.c',
  'tools/image_exif_viewer.c',
  'tools/image_metadata_inspector.c',
  'tools/image_crop.c',
  'tools/image_geometry.c',
  'tools/image_autocrop.c',
  'tools/image_perspective.c',
  'tools/image_brightness_contrast.c',
)

helvetia_image_lib = static_library('helvetia_image',
  image_sources,
  dependencies : [glib_dep, gtk_dep, adw_dep, exif_dep],
  include_directories : include_directories('.', '../..'),
  pic : true,
)
