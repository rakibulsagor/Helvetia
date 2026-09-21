#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>

G_BEGIN_DECLS

/* ------------------------------------------------------------------ */
/* File filters                                                       */
/* ------------------------------------------------------------------ */

GtkFileFilter *image_filter_images(void);
GtkFileFilter *image_filter_all(void);
GListStore    *image_filter_store_full(void);

/* ------------------------------------------------------------------ */
/* Drop zone — reusable widget for every Viewer tool                  */
/* ------------------------------------------------------------------ */

typedef void (*ImageDropCallback)(const char *path, gpointer user_data);

/*
 * Build a drop zone widget.
 *
 * The returned widget:
 *   - Shows a dashed border with an upload icon
 *   - Accepts click (opens file dialog)
 *   - Accepts drag-and-drop of a single image file
 *   - Calls on_file(path, user_data) when a file is chosen
 *   - Uses the filters from image_filter_store_full()
 */
GtkWidget *image_build_drop_zone(const char *hint_text,
                                  ImageDropCallback on_file,
                                  gpointer user_data);

/* ------------------------------------------------------------------ */
/* Toast helpers                                                      */
/* ------------------------------------------------------------------ */

void image_show_error(GtkWidget *widget, const char *message);
void image_show_info (GtkWidget *widget, const char *message);

/* ------------------------------------------------------------------ */
/* Utilities                                                          */
/* ------------------------------------------------------------------ */

/* Return TRUE if the path has a supported image extension. */
gboolean image_is_supported(const char *path);

/* Return the lowercase extension without the dot, or NULL. */
char *image_get_extension(const char *path);

/* Format a size in bytes as a human-readable string. */
char *image_format_size(guint64 bytes);

/* Save dialog — saves a pixbuf to disk with a file chooser */
void image_save_pixbuf_dialog(GtkWidget *parent,
                               GdkPixbuf *pixbuf,
                               const char *suggested_name);

/* Compute the currently displayed pixbuf as a GdkPixbuf from a GTK picture */
GdkPixbuf *image_picture_get_pixbuf(GtkPicture *picture);

G_END_DECLS
