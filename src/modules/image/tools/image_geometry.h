#pragma once
#include "../../../core/tool_registry.h"

GtkWidget *image_resize_create(void);
void image_resize_on_close(GtkWidget *view);

GtkWidget *image_rotate_create(void);
void image_rotate_on_close(GtkWidget *view);

GtkWidget *image_flip_create(void);
void image_flip_on_close(GtkWidget *view);

GtkWidget *image_canvas_resize_create(void);
void image_canvas_resize_on_close(GtkWidget *view);
