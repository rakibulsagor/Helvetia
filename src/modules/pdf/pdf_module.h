#pragma once
#include "../../core/plugin.h"
#include <gtk/gtk.h>

const HelvetiaModule *helvetia_pdf_get_module(void);

GtkWidget *build_pdf_to_text (void);
GtkWidget *build_pdf_to_image(void);
GtkWidget *build_pdf_merge   (void);
GtkWidget *build_pdf_split   (void);
