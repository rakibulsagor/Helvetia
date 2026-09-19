#pragma once
#include "../../core/plugin.h"
#include <gtk/gtk.h>

const HelvetiaModule *helvetia_pdf_get_module(void);

GtkWidget *build_pdf_to_text(void);
GtkWidget *build_pdf_to_image(void);
GtkWidget *build_pdf_merge(void);
GtkWidget *build_pdf_split(void);
GtkWidget *build_pdf_extract(void);
GtkWidget *build_pdf_delete(void);
GtkWidget *build_pdf_rotate(void);
GtkWidget *build_pdf_encrypt(void);
GtkWidget *build_pdf_decrypt(void);
GtkWidget *build_pdf_compress(void);
GtkWidget *build_pdf_viewer(void);
GtkWidget *build_pdf_thumbnails(void);
GtkWidget *build_image_to_pdf(void);
GtkWidget *build_pdf_to_html(void);
