#pragma once
#include "../../../core/tool_registry.h"

GtkWidget *image_thumbnail_grid_create(void);
extern const HelvetiaToolCommand image_thumbnail_grid_commands[];
void image_thumbnail_grid_on_close(GtkWidget *view);
