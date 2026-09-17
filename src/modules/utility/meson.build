utility_sources = files('utility_module.c')

utility_lib = static_library('helvetia_utility',
  utility_sources,
  dependencies        : [glib_dep, gtk_dep, math_dep],
  include_directories : include_directories('..', '../..'),
)
