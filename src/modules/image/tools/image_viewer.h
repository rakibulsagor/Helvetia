#pragma once
#include "../../../core/tool_registry.h"

GtkWidget *image_viewer_create(void);
extern const HelvetiaToolCommand image_viewer_commands[];
void image_viewer_on_close(GtkWidget *view);
