#pragma once
#include "../../../core/tool_registry.h"

GtkWidget *image_exposure_create(void);
extern const HelvetiaToolCommand image_exposure_commands[];
void image_exposure_on_close(GtkWidget *view);
