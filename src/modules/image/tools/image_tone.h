#pragma once
#include "../../../core/tool_registry.h"

GtkWidget *image_shadows_highlights_create(void);
GtkWidget *image_gamma_create(void);
GtkWidget *image_auto_enhance_create(void);
GtkWidget *image_histogram_create(void);

extern const HelvetiaToolCommand image_shadows_highlights_commands[];
extern const HelvetiaToolCommand image_gamma_commands[];
extern const HelvetiaToolCommand image_auto_enhance_commands[];
extern const HelvetiaToolCommand image_histogram_commands[];

void image_shadows_highlights_on_close(GtkWidget *view);
void image_gamma_on_close(GtkWidget *view);
void image_auto_enhance_on_close(GtkWidget *view);
void image_histogram_on_close(GtkWidget *view);
