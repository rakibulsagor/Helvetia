#pragma once
#include <gtk/gtk.h>

GtkWidget *image_straighten_create(void);
void image_straighten_on_close(GtkWidget *view);

GtkWidget *image_autocrop_create(void);
void image_autocrop_on_close(GtkWidget *view);
