#pragma once
#include "../../../core/tool_registry.h"

GtkWidget *image_brush_create(void);
GtkWidget *image_eraser_create(void);
GtkWidget *image_fill_create(void);
GtkWidget *image_gradient_create(void);
GtkWidget *image_text_create(void);
GtkWidget *image_shape_create(void);
GtkWidget *image_arrow_create(void);

extern const HelvetiaToolCommand image_brush_commands[];
extern const HelvetiaToolCommand image_eraser_commands[];
extern const HelvetiaToolCommand image_fill_commands[];
extern const HelvetiaToolCommand image_gradient_commands[];
extern const HelvetiaToolCommand image_text_commands[];
extern const HelvetiaToolCommand image_shape_commands[];
extern const HelvetiaToolCommand image_arrow_commands[];

void image_brush_on_close(GtkWidget *view);
void image_eraser_on_close(GtkWidget *view);
void image_fill_on_close(GtkWidget *view);
void image_gradient_on_close(GtkWidget *view);
void image_text_on_close(GtkWidget *view);
void image_shape_on_close(GtkWidget *view);
void image_arrow_on_close(GtkWidget *view);
