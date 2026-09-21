#pragma once
#include "../../../core/plugin.h"

GtkWidget *image_sepia_create(void);
GtkWidget *image_grayscale_create(void);
GtkWidget *image_invert_create(void);
GtkWidget *image_threshold_create(void);
GtkWidget *image_posterize_create(void);

extern const HelvetiaToolCommand image_sepia_commands[];
extern const HelvetiaToolCommand image_grayscale_commands[];
extern const HelvetiaToolCommand image_invert_commands[];
extern const HelvetiaToolCommand image_threshold_commands[];
extern const HelvetiaToolCommand image_posterize_commands[];

void image_sepia_on_close(GtkWidget *view);
void image_grayscale_on_close(GtkWidget *view);
void image_invert_on_close(GtkWidget *view);
void image_threshold_on_close(GtkWidget *view);
void image_posterize_on_close(GtkWidget *view);
