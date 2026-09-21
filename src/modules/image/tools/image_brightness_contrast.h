#pragma once
#include <gtk/gtk.h>
#include "../../../core/plugin.h"

GtkWidget *image_brightness_contrast_create(void);
extern const HelvetiaToolCommand image_brightness_contrast_commands[];
void image_brightness_contrast_on_close(GtkWidget *view);
