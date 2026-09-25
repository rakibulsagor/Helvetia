subdir('core')
subdir('ui')
subdir('modules')

helvetia_sources = [
  'main.c',
]

helvetia_pkg_deps = [
  gtk_dep,
  adw_dep,
  glib_dep,
  gmodule_dep,
  math_dep,
  gegl_dep,
  babl_dep,
  libraw_dep,
]

executable('helvetia',
  helvetia_sources,
  dependencies        : helvetia_pkg_deps,
  link_with           : [core_lib, ui_lib, modules_lib],
  include_directories : [include_directories('.'), config_inc],
  install             : true)
