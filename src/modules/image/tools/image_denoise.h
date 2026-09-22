#pragma once
#include "../../../core/plugin.h"

GtkWidget *image_noise_reduction_create(void);
GtkWidget *image_denoise_create(void);

extern const HelvetiaToolCommand image_noise_reduction_commands[];
extern const HelvetiaToolCommand image_denoise_commands[];

void image_noise_reduction_on_close(GtkWidget *view);
void image_denoise_on_close(GtkWidget *view);
