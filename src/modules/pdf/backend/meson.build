# src/modules/pdf/backend/meson.build

# Add C++ as a language for this target only.
# The rest of Helvetia stays pure C.
cpp = meson.get_compiler('cpp')

# libqpdf is a C++ library; find it via pkg-config.
qpdf_dep = dependency('libqpdf', version : '>= 10.0', required : false)

# Some distros ship qpdf without pkg-config metadata.
# Fallback: look for the library and infer headers.
if not qpdf_dep.found()
  qpdf_lib = cpp.find_library('qpdf', required : true)
  qpdf_dep = declare_dependency(
    dependencies : qpdf_lib,
    include_directories : include_directories('/usr/include/qpdf'),
  )
endif

# The backend sources — mix C and C++.
backend_sources = files(
  'qpdf_wrapper.cpp',
)

# Build a static library that the tool files link against.
# C++ standard: C++17 (qpdf 10+ requires it; qpdf 11 uses C++17 too).
helvetia_pdf_backend = static_library('helvetia_pdf_backend',
  backend_sources,
  dependencies : [glib_dep, qpdf_dep],
  include_directories : include_directories('.'),
  cpp_args : ['-std=c++17'],
  pic : true,
  install : false,
)

# Wrap in a dependency object so the tool files can link it cleanly.
helvetia_pdf_backend_dep = declare_dependency(
  link_with : helvetia_pdf_backend,
  include_directories : include_directories('.'),
  dependencies : [glib_dep],
)
