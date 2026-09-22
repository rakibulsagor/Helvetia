#pragma once
#include "../../../core/plugin.h"

GtkWidget *image_curves_create(void);
extern const HelvetiaToolCommand image_curves_commands[];
void image_curves_on_close(GtkWidget *view);
