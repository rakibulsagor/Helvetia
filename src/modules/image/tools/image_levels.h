#pragma once
#include <gtk/gtk.h>
#include "../../../core/plugin.h"

GtkWidget *image_levels_create(void);
extern const HelvetiaToolCommand image_levels_commands[];
void image_levels_on_close(GtkWidget *view);
