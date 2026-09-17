documents_sources = files('documents_module.c')

documents_lib = static_library('helvetia_documents',
  documents_sources,
  dependencies        : [glib_dep, gtk_dep],
  include_directories : include_directories('..', '../..')
)

modules_libs += documents_lib
