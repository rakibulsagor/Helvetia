task_sources = files(
  'task.c',
  'task_queue.c',
)

helvetia_tasks_lib = static_library('helvetia_tasks',
  task_sources,
  dependencies : [glib_dep, gio_dep, gobject_dep],
  include_directories : include_directories('.'),
  pic : true,
)

helvetia_tasks_dep = declare_dependency(
  link_with : helvetia_tasks_lib,
  include_directories : include_directories('.'),
  dependencies : [glib_dep, gio_dep, gobject_dep],
)