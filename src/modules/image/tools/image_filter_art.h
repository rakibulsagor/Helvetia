#pragma once
#include "../../../core/plugin.h"

GtkWidget *image_vignette_create(void);
GtkWidget *image_film_grain_create(void);
GtkWidget *image_glow_create(void);

extern const HelvetiaToolCommand image_vignette_commands[];
extern const HelvetiaToolCommand image_film_grain_commands[];
extern const HelvetiaToolCommand image_glow_commands[];

void image_vignette_on_close(GtkWidget *view);
void image_film_grain_on_close(GtkWidget *view);
void image_glow_on_close(GtkWidget *view);
