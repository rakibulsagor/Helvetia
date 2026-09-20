#pragma once
#include "../../core/plugin.h"
#include <gtk/gtk.h>

const HelvetiaModule *helvetia_devtools_get_module(void);

GtkWidget *build_uuid_generator(void);
GtkWidget *build_jwt_decoder(void);
GtkWidget *build_dev_timestamp(void);
GtkWidget *build_number_base_dev(void);
GtkWidget *build_http_status_lookup(void);
GtkWidget *build_hex_viewer(void);
