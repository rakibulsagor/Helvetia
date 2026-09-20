#pragma once
#include "../../../core/tool_registry.h"

GtkWidget *image_exif_viewer_create(void);
extern const HelvetiaToolCommand image_exif_viewer_commands[];
void image_exif_viewer_on_close(GtkWidget *view);
