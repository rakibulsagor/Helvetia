#pragma once
#include "../../core/plugin.h"
#include <gtk/gtk.h>

const HelvetiaModule *helvetia_convert_get_module(void);

/* Tool builders (convert_impl.c) */
GtkWidget *build_base64_tool          (void);
GtkWidget *build_url_encoder          (void);
GtkWidget *build_hex_encoder          (void);
GtkWidget *build_rot13                (void);
GtkWidget *build_json_formatter       (void);
GtkWidget *build_json_validator       (void);
GtkWidget *build_line_ending_converter(void);
GtkWidget *build_file_type_detector   (void);
