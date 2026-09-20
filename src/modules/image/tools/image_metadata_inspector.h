#pragma once
#include "../../../core/tool_registry.h"

GtkWidget *image_metadata_inspector_create(void);
extern const HelvetiaToolCommand image_metadata_inspector_commands[];
void image_metadata_inspector_on_close(GtkWidget *view);
