security_sources = files('security_module.c')

security_lib = static_library('helvetia_security',
  security_sources,
  dependencies        : [glib_dep, gtk_dep],
  include_directories : include_directories('..', '../..')
)

modules_libs += security_lib
