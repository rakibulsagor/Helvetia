#pragma once
#include "../../core/plugin.h"
#include <gtk/gtk.h>

const HelvetiaModule *helvetia_security_get_module(void);

GtkWidget *build_file_hash_calculator(void);
GtkWidget *build_text_hash_calculator(void);
GtkWidget *build_checksum_creator    (void);
GtkWidget *build_password_strength   (void);
GtkWidget *build_file_encrypt        (void);
GtkWidget *build_secure_delete       (void);
GtkWidget *build_cert_viewer         (void);
GtkWidget *build_ssh_keygen          (void);
