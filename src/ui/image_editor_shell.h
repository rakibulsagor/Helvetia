#pragma once
#include <adwaita.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct {
    GtkWidget *left_palette;   /* tool buttons */
    GtkWidget *center_canvas;  /* the picture + drawing area */
    GtkWidget *right_panels;   /* properties */
    GtkWidget *bottom_bar;     /* status, zoom, coordinates */
} ImageEditorSlots;

/* Builds a three-column + bottom-bar layout.
   Caller fills the empty slots. */
GtkWidget *image_editor_shell_new(ImageEditorSlots *out);

G_END_DECLS
