pdf_sources = files(
  'pdf_module.c',
  'pdf_shared.c',
  'tools/pdf_merge.c',
  'tools/pdf_split.c',
  'tools/pdf_compress.c',
  'backend/qpdf_wrapper.cpp',
  'pdf_impl.c',
)

pdf_lib = static_library('helvetia_pdf',
  pdf_sources,
  dependencies        : [glib_dep, gtk_dep, adw_dep],
  include_directories : include_directories('..', '../..')
)

modules_libs += pdf_lib
