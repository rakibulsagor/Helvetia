subdir('backend')

pdf_sources = files(
  'pdf_module.c',
  'pdf_shared.c',
  'tools/pdf_merge.c',
  'tools/pdf_split.c',
  'tools/pdf_compress.c',
  'pdf_impl.c',
)

pdf_lib = static_library('helvetia_pdf',
  pdf_sources,
  dependencies : [
    gtk_dep,
    adw_dep,
    glib_dep,
    helvetia_pdf_backend_dep,
  ],
  include_directories : include_directories('..', '../..'),
  pic : true,
)

modules_libs += pdf_lib

if get_option('build_tests')
  qpdf_test = executable('qpdf_wrapper_test',
    '../../../tests/qpdf_wrapper_test.c',
    dependencies : [glib_dep, helvetia_pdf_backend_dep],
    include_directories : include_directories('.', '../..'),
  )
  test('qpdf-wrapper', qpdf_test,
       args : ['pages', meson.current_source_dir() / 'test.pdf'],
       should_fail : false,
       timeout : 30)
endif
