#pragma once
#include "../../../core/tool_registry.h"

GtkWidget *image_saturation_vibrance_create(void);
GtkWidget *image_hue_shift_create(void);
GtkWidget *image_white_balance_create(void);
GtkWidget *image_color_balance_create(void);

extern const HelvetiaToolCommand image_saturation_vibrance_commands[];
extern const HelvetiaToolCommand image_hue_shift_commands[];
extern const HelvetiaToolCommand image_white_balance_commands[];
extern const HelvetiaToolCommand image_color_balance_commands[];

void image_saturation_vibrance_on_close(GtkWidget *view);
void image_hue_shift_on_close(GtkWidget *view);
void image_white_balance_on_close(GtkWidget *view);
void image_color_balance_on_close(GtkWidget *view);
