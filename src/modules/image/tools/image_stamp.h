#pragma once
#include "../../../core/tool_registry.h"

GtkWidget *image_eyedropper_create(void);
GtkWidget *image_dodge_burn_create(void);
GtkWidget *image_smudge_create(void);
GtkWidget *image_opacity_create(void);

extern const HelvetiaToolCommand image_eyedropper_commands[];
extern const HelvetiaToolCommand image_dodge_burn_commands[];
extern const HelvetiaToolCommand image_smudge_commands[];
extern const HelvetiaToolCommand image_opacity_commands[];

void image_eyedropper_on_close(GtkWidget *view);
void image_dodge_burn_on_close(GtkWidget *view);
void image_smudge_on_close(GtkWidget *view);
void image_opacity_on_close(GtkWidget *view);
