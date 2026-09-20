ui_sources = files(
  'window.c',
  'task_strip.c',
  'module_view.c',
  'widgets.c',
  'templates/template_file_processor.c',
  'templates/template_generator.c',
  'templates/template_converter.c',
  'templates/template_lookup.c',
  'templates/template_viewer.c',
  'templates/template_calculator.c'
)

ui_lib = static_library('helvetia_ui',
  ui_sources,
  dependencies        : [glib_dep, gtk_dep, adw_dep, helvetia_tasks_dep],
  include_directories : include_directories('.', '..', '../core'),
)
