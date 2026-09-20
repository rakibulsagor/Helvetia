db_sources = files(
  'db.c',
  'db_vault.c',
  'db_notes.c',
  'db_favorites.c',
)

helvetia_db_lib = static_library('helvetia_db',
  db_sources,
  dependencies : [
    glib_dep,
    dependency('sqlite3'),
    dependency('libsodium'),
  ],
  include_directories : include_directories('.'),
  pic : true,
)

helvetia_db_dep = declare_dependency(
  link_with : helvetia_db_lib,
  include_directories : include_directories('.'),
  dependencies : [glib_dep],
)

if get_option('build_tests')
  db_test = executable('db_test',
    files('../../../tests/db_test.c'),
    dependencies : [glib_dep, helvetia_db_dep, dependency('libsodium')],
    include_directories : include_directories('.'),
  )
  test('db', db_test)
endif
