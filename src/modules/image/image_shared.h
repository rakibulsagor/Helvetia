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

/*
 * Create a button that opens a file dialog and calls on_file with the
 * chosen image path. Used as the "New Image…" button in tool toolbars.
 * The callback is the same one passed to image_build_drop_zone().
 */
GtkWidget *image_new_image_button(ImageDropCallback on_file,
                                   gpointer          user_data);

/* ------------------------------------------------------------------ */
/* Ctrl+Z / Ctrl+R shortcuts                                          */
/* ------------------------------------------------------------------ */

typedef void (*ImageToolCallback)(GtkButton *btn, gpointer user_data);

void image_install_edit_shortcuts(GtkWidget         *root,
                                   ImageToolCallback  on_undo,
                                   ImageToolCallback  on_reset,
                                   gpointer           user_data);

/* ------------------------------------------------------------------ */
/* Undo / Reset support                                               */
/* ------------------------------------------------------------------ */

/* A stack of GdkPixbuf* — previous states of the image, oldest first.
   Every push adds a reference, every pop transfers a reference to the
   caller, every clear drops all references. */
GPtrArray *image_undo_stack_new(void);
void       image_undo_push(GPtrArray *stack, GdkPixbuf *pixbuf);
GdkPixbuf *image_undo_pop(GPtrArray *stack);
guint      image_undo_depth(GPtrArray *stack);
void       image_undo_clear(GPtrArray *stack);
void       image_undo_free(GPtrArray *stack);

/* Build an "Undo" button. It calls on_undo(user_data) when clicked.
   Returns a GtkButton* with the edit-undo-symbolic icon. */
GtkWidget *image_undo_button(GCallback on_undo, gpointer user_data);

/* Build a "Reset" button. It calls on_reset(user_data) when clicked. */
GtkWidget *image_reset_button(GCallback on_reset, gpointer user_data);

void image_zoom_in(GtkWidget *root);
void image_zoom_out(GtkWidget *root);
void image_zoom_reset(GtkWidget *root);
void image_install_zoom_shortcuts(GtkWidget *root);
void image_register_zoom(GtkWidget *root, GtkWidget *picture, double *zoom);

G_END_DECLS
