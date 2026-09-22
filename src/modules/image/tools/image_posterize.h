#pragma once
#include "../../../core/plugin.h"

GtkWidget *image_posterize_create(void);
GtkWidget *image_threshold_create(void);
GtkWidget *image_vignette_create(void);

extern const HelvetiaToolCommand image_posterize_commands[];
extern const HelvetiaToolCommand image_threshold_commands[];
extern const HelvetiaToolCommand image_vignette_commands[];

void image_posterize_on_close(GtkWidget *view);
void image_threshold_on_close(GtkWidget *view);
void image_vignette_on_close(GtkWidget *view);
