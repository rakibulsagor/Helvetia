#pragma once
#include "../../core/plugin.h"
#include <gtk/gtk.h>

const HelvetiaModule *helvetia_images_get_module(void);

GtkWidget *build_image_viewer    (void);
GtkWidget *build_image_converter (void);
GtkWidget *build_image_resize    (void);
GtkWidget *build_image_grayscale (void);
GtkWidget *build_image_invert    (void);
GtkWidget *build_image_crop      (void);
GtkWidget *build_image_blur      (void);
