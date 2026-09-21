#pragma once
#include "../../../core/tool_registry.h"

GtkWidget *image_straighten_create(void);
void image_straighten_on_close(GtkWidget *view);

GtkWidget *image_autocrop_create(void);
void image_autocrop_on_close(GtkWidget *view);
