ui_sources = files(
  'window.c',
  'module_view.c',
  'widgets.c'
)

ui_lib = static_library('helvetia_ui',
  ui_sources,
  dependencies        : [glib_dep, gtk_dep, adw_dep],
  include_directories : include_directories('..'),
)
