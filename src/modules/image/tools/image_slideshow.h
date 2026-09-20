#pragma once
#include "../../../core/tool_registry.h"

GtkWidget *image_slideshow_create(void);
extern const HelvetiaToolCommand image_slideshow_commands[];
void image_slideshow_on_close(GtkWidget *view);
