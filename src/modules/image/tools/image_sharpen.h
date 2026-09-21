#pragma once
#include "../../../core/tool_registry.h"

GtkWidget *image_blur_create(void);
GtkWidget *image_sharpen_create(void);
GtkWidget *image_unsharp_mask_create(void);

extern const HelvetiaToolCommand image_blur_commands[];
extern const HelvetiaToolCommand image_sharpen_commands[];
extern const HelvetiaToolCommand image_unsharp_mask_commands[];

void image_blur_on_close(GtkWidget *view);
void image_sharpen_on_close(GtkWidget *view);
void image_unsharp_mask_on_close(GtkWidget *view);
