#include "security_module.h"
#include <stddef.h>

static const HelvetiaTool tools_encryption[] = {
    { .id = "file_encrypt", .name = "File Encrypt", .description = "AES-256, ChaCha20", .icon_name = "dialog-password-symbolic", .keywords = (const char*[]){ "security", "file", "encrypt", NULL }, .cli_command = "file-encrypt", .create_view = build_file_encrypt },
    { .id = "file_decrypt", .name = "File Decrypt", .description = "File Decrypt", .icon_name = "dialog-password-symbolic", .keywords = (const char*[]){ "security", "decrypt", "file", NULL }, .cli_command = "file-decrypt", .create_view = build_file_decrypt },
    { .id = "folder_encrypt", .name = "Folder Encrypt", .description = "Folder Encrypt", .icon_name = "dialog-password-symbolic", .keywords = (const char*[]){ "folder", "security", "encrypt", NULL }, .cli_command = "folder-encrypt", .create_view = build_folder_encrypt },
    { .id = "folder_decrypt", .name = "Folder Decrypt", .description = "Folder Decrypt", .icon_name = "dialog-password-symbolic", .keywords = (const char*[]){ "folder", "security", "decrypt", NULL }, .cli_command = "folder-decrypt", .create_view = build_folder_decrypt },
    { 0 }
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
