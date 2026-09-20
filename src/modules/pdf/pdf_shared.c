#include "pdf_shared.h"

#include "../../../core/tasks/task_queue.h"

/* ------------------------------------------------------------------ */
/* Filters                                                            */
/* ------------------------------------------------------------------ */

GtkFileFilter *pdf_filter_pdf(void) {
    GtkFileFilter *f = gtk_file_filter_new();
    gtk_file_filter_set_name(f, "PDF files");
    gtk_file_filter_add_pattern(f, "*.pdf");
    gtk_file_filter_add_mime_type(f, "application/pdf");
    return f;
}

GtkFileFilter *pdf_filter_images(void) {
    GtkFileFilter *f = gtk_file_filter_new();
    gtk_file_filter_set_name(f, "Images");
    gtk_file_filter_add_pattern(f, "*.png");
    gtk_file_filter_add_pattern(f, "*.jpg");
    gtk_file_filter_add_pattern(f, "*.jpeg");
    gtk_file_filter_add_pattern(f, "*.tif");
    gtk_file_filter_add_pattern(f, "*.tiff");
    gtk_file_filter_add_pattern(f, "*.webp");
    return f;
}

GtkFileFilter *pdf_filter_all(void) {
    GtkFileFilter *f = gtk_file_filter_new();
    gtk_file_filter_set_name(f, "All files");
    gtk_file_filter_add_pattern(f, "*");
    return f;
}

GListStore *pdf_filter_store_full(void) {
    GListStore *store = g_list_store_new(GTK_TYPE_FILE_FILTER);
    g_list_store_append(store, pdf_filter_pdf());
    g_list_store_append(store, pdf_filter_images());
    g_list_store_append(store, pdf_filter_all());
    return store;
}

/* ------------------------------------------------------------------ */
/* Toasts                                                             */
/* ------------------------------------------------------------------ */

static AdwToastOverlay *find_toast_overlay(GtkWidget *widget) {
    GtkWidget *root = GTK_WIDGET(gtk_widget_get_root(widget));
    if (!root) return NULL;

    /* Walk the widget tree looking for an AdwToastOverlay.
       In practice, this is the direct child of the AdwApplicationWindow. */
    GtkWidget *child = gtk_window_get_child(GTK_WINDOW(root));
    while (child) {
        if (ADW_IS_TOAST_OVERLAY(child))
            return ADW_TOAST_OVERLAY(child);

        /* Descend into AdwToolbarView or similar containers */
        GtkWidget *next = NULL;
        if (ADW_IS_TOOLBAR_VIEW(child))
            next = adw_toolbar_view_get_content(ADW_TOOLBAR_VIEW(child));
        else if (ADW_IS_TOAST_OVERLAY(child))
            next = adw_toast_overlay_get_child(ADW_TOAST_OVERLAY(child));

        child = next ? next : gtk_widget_get_first_child(child);
    }
    return NULL;
}

void pdf_show_error(GtkWidget *widget, const char *message) {
    AdwToastOverlay *overlay = find_toast_overlay(widget);
    if (overlay) {
        AdwToast *toast = adw_toast_new(message);
        adw_toast_set_timeout(toast, 5);
        adw_toast_overlay_add_toast(overlay, toast);
    } else {
        g_warning("pdf: %s", message);
    }
}

void pdf_show_info(GtkWidget *widget, const char *message) {
    AdwToastOverlay *overlay = find_toast_overlay(widget);
    if (overlay) {
        AdwToast *toast = adw_toast_new(message);
        adw_toast_set_timeout(toast, 3);
        adw_toast_overlay_add_toast(overlay, toast);
    }
}

/* ------------------------------------------------------------------ */
/* Path helpers                                                       */
/* ------------------------------------------------------------------ */

char *pdf_format_size(guint64 bytes) {
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    double value = (double)bytes;
    int unit = 0;
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        unit++;
    }
    return g_strdup_printf("%.1f %s", value, units[unit]);
}

char *pdf_stem(const char *path) {
    char *base = g_path_get_basename(path);
    char *dot  = strrchr(base, '.');
    if (dot && dot != base) *dot = '\0';
    return base;
}

char *pdf_join_path(const char *dir, const char *name) {
    return g_build_filename(dir, name, NULL);
}

/* ------------------------------------------------------------------ */
/* Task runner                                                        */
/* ------------------------------------------------------------------ */

typedef struct {
    PdfTaskFunc    func;
    gpointer       data;
    GDestroyNotify free_func;
    GtkWidget     *tool_view;
} PdfWorkerCtx;

static void pdf_worker_ctx_free(PdfWorkerCtx *ctx) {
    if (ctx->free_func) ctx->free_func(ctx->data);
    g_free(ctx);
}

static void pdf_worker(GTask *gtask, gpointer source_object,
                       gpointer task_data, GCancellable *cancellable) {
    (void)source_object;
    PdfWorkerCtx *ctx = task_data;
    ctx->func(gtask, ctx->tool_view, ctx->data, cancellable);
}

static void on_pdf_task_completed(HelvetiaTask *task, const char *error,
                                  gpointer user_data) {
    GtkWidget *tool_view = user_data;
    if (error)
        pdf_show_error(tool_view, error);
    else
        pdf_show_info(tool_view, helvetia_task_get_name(task));
}

void pdf_run_task_async(GtkWidget      *tool_view,
                        const char     *description,
                        PdfTaskFunc     func,
                        gpointer        task_data,
                        GDestroyNotify  task_data_free,
                        GAsyncReadyCallback done) {
    (void)done;

    HelvetiaTask *task = helvetia_task_new(description, "pdf");

    PdfWorkerCtx *ctx = g_new0(PdfWorkerCtx, 1);
    ctx->func      = func;
    ctx->data      = task_data;
    ctx->free_func = task_data_free;
    ctx->tool_view = tool_view;

    helvetia_task_set_worker(task, pdf_worker, ctx,
                             (GDestroyNotify)pdf_worker_ctx_free);

    /* Submit to the global queue. The queue owns the reference now. */
    HelvetiaTaskQueue *queue = helvetia_task_queue_get_default();
    helvetia_task_queue_submit(queue, task);

    /* Show a toast on completion, using the window's toast overlay.
       Auto-disconnects if the tool view is destroyed first. */
    g_signal_connect_object(task, "completed",
                            G_CALLBACK(on_pdf_task_completed), tool_view, 0);
}
