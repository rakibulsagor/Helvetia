#include "pdf_shared.h"

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
    GtkWidget     *tool_view;
    char          *description;
    PdfTaskFunc    func;
    gpointer       task_data;
    GDestroyNotify task_data_free;
    GAsyncReadyCallback done;
} PdfTaskContext;

static void pdf_task_context_free(PdfTaskContext *ctx) {
    g_free(ctx->description);
    if (ctx->task_data_free)
        ctx->task_data_free(ctx->task_data);
    g_free(ctx);
}

static void pdf_task_thread(GTask *task, gpointer source_object,
                            gpointer task_data, GCancellable *cancellable) {
    PdfTaskContext *ctx = (PdfTaskContext*)task_data;
    (void)source_object;
    ctx->func(task, ctx->tool_view, ctx->task_data, cancellable);
}

static void pdf_task_done(GObject *source, GAsyncResult *result, gpointer user_data) {
    GTask *task = G_TASK(result);
    PdfTaskContext *ctx = (PdfTaskContext*)user_data;

    if (ctx->done)
        ctx->done(source, result, ctx->tool_view);

    /* The GTask was created with a GDestroyNotify that runs
       pdf_task_context_free when the task is finalized. */
    (void)task;
}

void pdf_run_task_async(GtkWidget  *tool_view,
                        const char *description,
                        PdfTaskFunc func,
                        gpointer    task_data,
                        GDestroyNotify task_data_free,
                        GAsyncReadyCallback done) {
    PdfTaskContext *ctx = g_new0(PdfTaskContext, 1);
    ctx->tool_view      = tool_view;
    ctx->description    = g_strdup(description);
    ctx->func           = func;
    ctx->task_data      = task_data;
    ctx->task_data_free = task_data_free;
    ctx->done           = done;

    GTask *task = g_task_new(NULL, NULL, pdf_task_done, ctx);
    g_task_set_task_data(task,
                         ctx,
                         (GDestroyNotify)pdf_task_context_free);
    g_task_run_in_thread(task, pdf_task_thread);
    g_object_unref(task);

    /* Optionally register with the window's task queue */
    PdfTask *t = g_new0(PdfTask, 1);
    t->status = PDF_TASK_RUNNING;
    t->description = g_strdup(description);
    pdf_task_register(tool_view, t);
}

/* ------------------------------------------------------------------ */
/* Task queue integration                                             */
/* ------------------------------------------------------------------ */

/* The task queue is a bottom bar owned by the window. In this pass
   we just emit a signal that the window listens for. A later pass
   replaces this with a proper GTK list model. */
void pdf_task_register(GtkWidget *tool_view, PdfTask *task) {
    GtkWidget *root = GTK_WIDGET(gtk_widget_get_root(tool_view));
    if (!root) return;

    /* g_signal_emit_by_name(root, "task-started", task); */
}

void pdf_task_complete(PdfTask *task, const char *error) {
    task->status = error ? PDF_TASK_FAILED : PDF_TASK_DONE;
    if (error)
        task->error = g_strdup(error);
    /* Real implementation notifies the window */
}
