#pragma once
#include "../../../core/tool_registry.h"
GtkWidget *image_crop_create(void);
extern const HelvetiaToolCommand image_crop_commands[];
void image_crop_on_close(GtkWidget *view);
