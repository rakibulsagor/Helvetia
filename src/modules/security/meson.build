security_sources = files('security_module.c', 'security_impl.c', 'file_encrypt.c', 'file_decrypt.c', 'folder_encrypt.c', 'folder_decrypt.c', 'helv_crypto.c')

security_lib = static_library('helvetia_security',
  security_sources,
  dependencies        : [glib_dep, gtk_dep, adw_dep, openssl_dep, libsodium_dep, libarchive_dep],
  include_directories : include_directories('..', '../..')
)

modules_libs += security_lib
