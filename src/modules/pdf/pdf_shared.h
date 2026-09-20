#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include <glib.h>

G_BEGIN_DECLS

/* ------------------------------------------------------------------ */
/* Shared helpers used by every PDF tool                              */
/* ------------------------------------------------------------------ */

/* Build a standard PDF file filter for GtkFileDialog. */
GtkFileFilter *pdf_filter_pdf(void);
GtkFileFilter *pdf_filter_images(void);
GtkFileFilter *pdf_filter_all(void);

/* Build a list store containing the filters above (for dialogs
   that accept multiple filters). */
GListStore    *pdf_filter_store_full(void);

/* Show an error toast on the window that owns `widget`. */
void           pdf_show_error(GtkWidget *widget, const char *message);
void           pdf_show_info(GtkWidget *widget, const char *message);

/* Format a byte count as a human-readable string ("2.4 MB"). */
char          *pdf_format_size(guint64 bytes);

/* Return the basename of a path without the extension. */
char          *pdf_stem(const char *path);

/* Join a directory and a filename. */
char          *pdf_join_path(const char *dir, const char *name);

/* ------------------------------------------------------------------ */
/* Task-runner helpers — every tool uses these for background work    */
/* ------------------------------------------------------------------ */

typedef void (*PdfTaskFunc)(GTask *task, gpointer source_object,
                            gpointer task_data, GCancellable *cancellable);

/* Run `func` on a background thread via the global task queue. The
   tool view is passed through unchanged so the completion handler can
   update the UI. A completion toast (success or error) is shown
   automatically in the window that owns `tool_view`. */
void pdf_run_task_async(GtkWidget  *tool_view,
                        const char *description,
                        PdfTaskFunc func,
                        gpointer    task_data,
                        GDestroyNotify task_data_free,
                        GAsyncReadyCallback done);

G_END_DECLS
