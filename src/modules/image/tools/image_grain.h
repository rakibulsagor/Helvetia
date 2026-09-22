#pragma once
#include "../../../core/plugin.h"

GtkWidget *image_film_grain_create(void);
GtkWidget *image_glow_create(void);
GtkWidget *image_emboss_create(void);
GtkWidget *image_edge_detect_create(void);
GtkWidget *image_pixelate_create(void);
GtkWidget *image_mosaic_create(void);

extern const HelvetiaToolCommand image_film_grain_commands[];
extern const HelvetiaToolCommand image_glow_commands[];
extern const HelvetiaToolCommand image_emboss_commands[];
extern const HelvetiaToolCommand image_edge_detect_commands[];
extern const HelvetiaToolCommand image_pixelate_commands[];
extern const HelvetiaToolCommand image_mosaic_commands[];

void image_film_grain_on_close(GtkWidget *view);
void image_glow_on_close(GtkWidget *view);
void image_emboss_on_close(GtkWidget *view);
void image_edge_detect_on_close(GtkWidget *view);
void image_pixelate_on_close(GtkWidget *view);
void image_mosaic_on_close(GtkWidget *view);
