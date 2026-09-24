#include "security_module.h"
#include <stddef.h>

static const HelvetiaTool tools_encryption[] = {
    { "file_encrypt", "File Encrypt", "AES-256, ChaCha20", "dialog-password-symbolic", (const char*[]){ "security", "file", "encrypt", NULL }, "file-encrypt", build_file_encrypt },
    { "file_decrypt", "File Decrypt", "File Decrypt", "dialog-password-symbolic", (const char*[]){ "security", "decrypt", "file", NULL }, "file-decrypt", build_file_decrypt },
    { "folder_encrypt", "Folder Encrypt", "Folder Encrypt", "dialog-password-symbolic", (const char*[]){ "folder", "security", "encrypt", NULL }, "folder-encrypt", build_folder_encrypt },
    { "folder_decrypt", "Folder Decrypt", "Folder Decrypt", "dialog-password-symbolic", (const char*[]){ "folder", "security", "decrypt", NULL }, "folder-decrypt", build_folder_decrypt },
    { NULL }
};

static const HelvetiaSubcategory security_subcategories[] = {
    { "Encryption", tools_encryption },
    { NULL, NULL }
};

static const HelvetiaModule security_module = {
    .id            = "security",
    .name          = "Security",
    .icon_name     = "dialog-password-symbolic",
    .description   = "Encrypt, hash, and secure your files.",
    .subcategories = security_subcategories,
    .create_view   = NULL,
    .on_activate   = NULL,
    .on_deactivate = NULL,
    .on_shutdown   = NULL,
};

const HelvetiaModule *helvetia_security_get_module(void) {
    return &security_module;
}
