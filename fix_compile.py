import os

def fix_image_shared():
    path = "src/modules/image/image_shared.c"
    with open(path, "r") as f: content = f.read()
    content = content.replace("GtkWidget *root = gtk_widget_get_root(widget);", "GtkRoot *root = gtk_widget_get_root(widget);")
    with open(path, "w") as f: f.write(content)

def fix_image_viewer():
    path = "src/modules/image/tools/image_viewer.c"
    with open(path, "r") as f: content = f.read()
    
    # Remove cmd_open entirely as cmd_open_reuse will replace it
    start_idx = content.find("static void cmd_open(GtkWidget *view)")
    end_idx = content.find("static void cmd_open_reuse(GtkWidget *view)")
    if start_idx != -1 and end_idx != -1:
        content = content[:start_idx] + content[end_idx:]
    
    content = content.replace("static void cmd_open_reuse(GtkWidget *view)", "static void cmd_open(GtkWidget *view)")
    content = content.replace(".activate = cmd_open_reuse", ".activate = cmd_open")
    
    # Fix lambda
    lambda_str = """    GAsyncReadyCallback cb = (GAsyncReadyCallback)+[](
        GObject *source, GAsyncResult *result, gpointer data) {
        OpenCtx *c = data;
        GtkWidget *v = c->view;
        ImageViewerState *s = get_state(v);

        GError *err = NULL;
        GFile *file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source),
                                                  result, &err);
        if (err) {
            if (!g_error_matches(err, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
                image_show_error(v, err->message);
            g_error_free(err);
            g_free(c);
            return;
        }

        char *path = g_file_get_path(file);
        g_object_unref(file);

        if (path && image_is_supported(path))
            load_image(s, path);

        g_free(path);
        g_free(c);
    };"""
    
    new_callback = """static void on_open_dialog_finished(GObject *source, GAsyncResult *result, gpointer data) {
    OpenCtx *c = data;
    GtkWidget *v = c->view;
    ImageViewerState *s = get_state(v);
    GError *err = NULL;
    GFile *file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source), result, &err);
    if (err) {
        if (!g_error_matches(err, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
            image_show_error(v, err->message);
        g_error_free(err);
        g_free(c);
        return;
    }
    char *path = g_file_get_path(file);
    g_object_unref(file);
    if (path && image_is_supported(path))
        load_image(s, path);
    g_free(path);
    g_free(c);
}"""

    # We need to insert `typedef struct { GtkWidget *view; } OpenCtx;` and `new_callback` above cmd_open.
    content = content.replace("    typedef struct { GtkWidget *view; } OpenCtx;", "")
    content = content.replace(lambda_str, "    GAsyncReadyCallback cb = on_open_dialog_finished;")
    
    if "typedef struct { GtkWidget *view; } OpenCtx;" not in content:
        content = content.replace("static void cmd_open(GtkWidget *view) {", 
                                  "typedef struct { GtkWidget *view; } OpenCtx;\n" + new_callback + "\n\nstatic void cmd_open(GtkWidget *view) {")

    with open(path, "w") as f: f.write(content)

def fix_image_thumbnail_grid():
    path = "src/modules/image/tools/image_thumbnail_grid.c"
    with open(path, "r") as f: content = f.read()

    # Open dialog
    lambda_open = """        (GAsyncReadyCallback)+[](GObject *src, GAsyncResult *res, gpointer data) {
            OC *c = data;
            GError *e = NULL;
            GFile *f = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(src), res, &e);
            if (e) { g_error_free(e); g_free(c); return; }
            char *path = g_file_get_path(f);
            g_object_unref(f);
            if (path) load_folder(c->view, path);
            g_free(path);
            g_free(c);
        }"""
    cb_open = """static void on_open_dialog_finished(GObject *src, GAsyncResult *res, gpointer data) {
    OC *c = data;
    GError *e = NULL;
    GFile *f = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(src), res, &e);
    if (e) { g_error_free(e); g_free(c); return; }
    char *path = g_file_get_path(f);
    g_object_unref(f);
    if (path) load_folder(c->view, path);
    g_free(path);
    g_free(c);
}"""
    content = content.replace("    typedef struct { GtkWidget *view; } OC;", "")
    content = content.replace(lambda_open, "on_open_dialog_finished")
    if "typedef struct { GtkWidget *view; } OC;" not in content:
        content = content.replace("static void on_open(GtkButton *b, gpointer d) {", 
                                  "typedef struct { GtkWidget *view; } OC;\n" + cb_open + "\n\nstatic void on_open(GtkButton *b, gpointer d) {")

    # Export dialog
    lambda_export = """        (GAsyncReadyCallback)+[](GObject *src, GAsyncResult *res, gpointer data) {
            Ctx *c = data;
            GError *e = NULL;
            GFile *f = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(src), res, &e);
            if (e) { g_error_free(e); sheet_job_free(c->job); g_free(c); return; }

            c->job->out_path = g_file_get_path(f);
            g_object_unref(f);

            GTask *t = g_task_new(NULL, NULL, sheet_done, c->view);
            g_task_set_task_data(t, c->job, (GDestroyNotify)sheet_job_free);
            g_task_run_in_thread(t, sheet_worker);
            g_object_unref(t);
            g_free(c);
        }"""
    cb_export = """static void on_export_dialog_finished(GObject *src, GAsyncResult *res, gpointer data) {
    Ctx *c = data;
    GError *e = NULL;
    GFile *f = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(src), res, &e);
    if (e) { g_error_free(e); sheet_job_free(c->job); g_free(c); return; }
    c->job->out_path = g_file_get_path(f);
    g_object_unref(f);
    GTask *t = g_task_new(NULL, NULL, sheet_done, c->view);
    g_task_set_task_data(t, c->job, (GDestroyNotify)sheet_job_free);
    g_task_run_in_thread(t, sheet_worker);
    g_object_unref(t);
    g_free(c);
}"""
    content = content.replace("    typedef struct { SheetJob *job; GtkWidget *view; } Ctx;", "")
    content = content.replace(lambda_export, "on_export_dialog_finished")
    if "typedef struct { SheetJob *job; GtkWidget *view; } Ctx;" not in content:
        content = content.replace("static void export_sheet(GtkWidget *view) {", 
                                  "typedef struct { SheetJob *job; GtkWidget *view; } Ctx;\n" + cb_export + "\n\nstatic void export_sheet(GtkWidget *view) {")

    with open(path, "w") as f: f.write(content)

def fix_image_exif_viewer():
    path = "src/modules/image/tools/image_exif_viewer.c"
    with open(path, "r") as f: content = f.read()

    lambda_open = """        (GAsyncReadyCallback)+[](GObject *src, GAsyncResult *res, gpointer data) {
            OC *c = data;
            GError *e = NULL;
            GFile *f = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(src), res, &e);
            if (e) { g_error_free(e); g_free(c); return; }
            char *path = g_file_get_path(f);
            g_object_unref(f);
            if (path) render_exif(c->view, path);
            g_free(path);
            g_free(c);
        }"""
    cb_open = """static void on_open_dialog_finished(GObject *src, GAsyncResult *res, gpointer data) {
    OC *c = data;
    GError *e = NULL;
    GFile *f = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(src), res, &e);
    if (e) { g_error_free(e); g_free(c); return; }
    char *path = g_file_get_path(f);
    g_object_unref(f);
    if (path) render_exif(c->view, path);
    g_free(path);
    g_free(c);
}"""
    content = content.replace("    typedef struct { GtkWidget *view; } OC;", "")
    content = content.replace(lambda_open, "on_open_dialog_finished")
    if "typedef struct { GtkWidget *view; } OC;" not in content:
        content = content.replace("static void cmd_open(GtkWidget *v) {", 
                                  "typedef struct { GtkWidget *view; } OC;\n" + cb_open + "\n\nstatic void cmd_open(GtkWidget *v) {")

    with open(path, "w") as f: f.write(content)

if __name__ == "__main__":
    fix_image_shared()
    fix_image_viewer()
    fix_image_thumbnail_grid()
    fix_image_exif_viewer()
