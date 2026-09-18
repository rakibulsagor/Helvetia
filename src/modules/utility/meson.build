utility_sources = files(
  'utility_module.c', 
  'utility_extra.c', 
  'scicalc_parser.c', 
  'color_studio.c', 
  'qr_studio.c', 
  'qrcodegen.c',
  'quirc/quirc.c',
  'quirc/decode.c',
  'quirc/identify.c',
  'quirc/version_db.c',
  'timezone_studio.c',
  'time_picker.c',
  'math_calculators.c'
)

utility_lib = static_library('helvetia_utility',
  utility_sources,
  dependencies        : [glib_dep, gtk_dep, math_dep],
  include_directories : include_directories('..', '../..'),
)
