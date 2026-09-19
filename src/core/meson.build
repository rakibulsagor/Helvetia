core_sources = files(
  'module_registry.c',
  'plugin_loader.c',
  'tool_registry.c',
  'favorites.c',
)

core_lib = static_library('helvetia_core',
  core_sources,
  dependencies        : [glib_dep, gtk_dep, gmodule_dep],
  include_directories : include_directories('.'),
)
