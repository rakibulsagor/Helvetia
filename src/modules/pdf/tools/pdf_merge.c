#include "../pdf_tools.h"
#include "../pdf_shared.h"
#include "../backend/qpdf_wrapper.h"

/* =========================================================================
 * STATE
 * ========================================================================= */

typedef struct {
    GtkWidget *list_box;     /* Shows files to merge */
    GtkWidget *empty_state;  /* Shown when no files added */
    GPtrArray *files;        /* Array of string paths */
    char      *output_path;  /* Where to save */
} MergeState;

static void merge_state_free(gpointer data) {
    MergeState *state = data;
    if (state->files) g_ptr_array_unref(state->files);
    g_free(state->output_path);
    g_free(state);
}

static MergeState *get_state(GtkWidget *view) {
    return g_object_get_data(G_OBJECT(view), "merge-state");
}

/* =========================================================================
 * UI HELPERS
 * ========================================================================= */

static void update_ui(MergeState *state) {
    gboolean has_files = state->files->len > 0;
    gtk_widget_set_visible(state->list_box, has_files);
    gtk_widget_set_visible(state->empty_state, !has_files);
}

static void on_remove_row(GtkButton *btn, gpointer user_data) {
    GtkWidget *row = user_data;
    GtkWidget *list = gtk_widget_get_parent(row);
    MergeState *state = get_state(list);

    int index = gtk_list_box_row_get_index(GTK_LIST_BOX_ROW(row));
    g_ptr_array_remove_index(state->files, index);
    gtk_list_box_remove(GTK_LIST_BOX(list), row);
    update_ui(state);
}

static void add_file_row(MergeState *state, const char *path) {
    g_ptr_array_add(state->files, g_strdup(path));

    GtkWidget *row = gtk_list_box_row_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_start(box, 12);
    gtk_widget_set_margin_end(box, 12);
    gtk_widget_set_margin_top(box, 12);
    gtk_widget_set_margin_bottom(box, 12);

    GtkWidget *icon = gtk_image_new_from_icon_name("application-pdf-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 32);

    char *stem = pdf_stem(path);
    GtkWidget *lbl = gtk_label_new(stem);
    g_free(stem);
    
    gtk_widget_set_hexpand(lbl, TRUE);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);

    GtkWidget *del_btn = gtk_button_new_from_icon_name("user-trash-symbolic");
    gtk_widget_add_css_class(del_btn, "flat");
    gtk_widget_add_css_class(del_btn, "destructive-action");
    g_signal_connect(del_btn, "clicked", G_CALLBACK(on_remove_row), row);

    gtk_box_append(GTK_BOX(box), icon);
    gtk_box_append(GTK_BOX(box), lbl);
    gtk_box_append(GTK_BOX(box), del_btn);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
    
    gtk_list_box_append(GTK_LIST_BOX(state->list_box), row);
}

/* =========================================================================
 * COMMANDS & WORKERS
 * ========================================================================= */

static void worker(GTask *task, gpointer source_object,
                   gpointer task_data, GCancellable *cancellable) {
    MergeState *state = task_data;
    (void)source_object;
    (void)cancellable;

    GError *err = NULL;
    gboolean ok = qpdf_merge_files((const char *const *)state->files->pdata,
                                   state->files->len,
                                   state->output_path,
                                   &err);

    if (!ok) {
        g_task_return_error(task, err);
    } else {
        g_task_return_boolean(task, TRUE);
    }
}

static void on_done(GObject *source, GAsyncResult *res, gpointer user_data) {
    (void)source;
    GtkWidget *view = user_data;
    GError *err = NULL;
    gboolean ok = g_task_propagate_boolean(G_TASK(res), &err);

    if (!ok) {
        pdf_show_error(view, err->message);
        g_error_free(err);
    } else {
        pdf_show_info(view, "PDFs merged successfully");
        MergeState *state = get_state(view);
        g_ptr_array_set_size(state->files, 0);
        
        GtkWidget *child;
        while ((child = gtk_widget_get_first_child(state->list_box))) {
            gtk_list_box_remove(GTK_LIST_BOX(state->list_box), child);
        }
        update_ui(state);
    }
}

/* -------------------------------------------------------------------------
 * ADD FILES
 * ------------------------------------------------------------------------- */
static void on_files_chosen(GObject *source, GAsyncResult *res, gpointer user_data) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source);
    GtkWidget *view = user_data;
    MergeState *state = get_state(view);

    GListModel *files = gtk_file_dialog_open_multiple_finish(dialog, res, NULL);
    if (!files) return;

    guint n = g_list_model_get_n_items(files);
    for (guint i = 0; i < n; i++) {
        GFile *file = g_list_model_get_item(files, i);
        char *path = g_file_get_path(file);
        if (path) {
            add_file_row(state, path);
            g_free(path);
        }
        g_object_unref(file);
    }
    g_object_unref(files);
    update_ui(state);
}

static void cmd_add(GtkWidget *view) {
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Add PDFs");
    gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(pdf_filter_store_full()));

    GtkWindow *parent = GTK_WINDOW(gtk_widget_get_root(view));
    gtk_file_dialog_open_multiple(dialog, parent, NULL, on_files_chosen, view);
    g_object_unref(dialog);
}

/* -------------------------------------------------------------------------
 * SAVE / MERGE
 * ------------------------------------------------------------------------- */
static void on_save_chosen(GObject *source, GAsyncResult *res, gpointer user_data) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source);
    GtkWidget *view = user_data;
    MergeState *state = get_state(view);

    GFile *file = gtk_file_dialog_save_finish(dialog, res, NULL);
    if (!file) return;

    g_free(state->output_path);
    state->output_path = g_file_get_path(file);
    g_object_unref(file);

    /* Run backend */
    pdf_run_task_async(view, "Merging PDFs…", worker, state, NULL, on_done);
}

static void cmd_merge(GtkWidget *view) {
    MergeState *state = get_state(view);
    if (state->files->len < 2) {
        pdf_show_error(view, "Add at least two PDFs to merge.");
        return;
    }

    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Save Merged PDF");
    gtk_file_dialog_set_initial_name(dialog, "Merged.pdf");

    GtkWindow *parent = GTK_WINDOW(gtk_widget_get_root(view));
    gtk_file_dialog_save(dialog, parent, NULL, on_save_chosen, view);
    g_object_unref(dialog);
}

/* =========================================================================
 * VIEW LIFECYCLE
 * ========================================================================= */

GtkWidget *pdf_merge_create_view(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    /* Empty state */
    GtkWidget *empty = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_valign(empty, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(empty, GTK_ALIGN_CENTER);
    gtk_widget_set_vexpand(empty, TRUE);

    GtkWidget *img = gtk_image_new_from_icon_name("application-pdf-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(img), 64);
    GtkWidget *lbl = gtk_label_new("No PDFs Added");
    gtk_widget_add_css_class(lbl, "title-2");

    gtk_box_append(GTK_BOX(empty), img);
    gtk_box_append(GTK_BOX(empty), lbl);

    /* List box */
    GtkWidget *list = gtk_list_box_new();
    gtk_widget_add_css_class(list, "boxed-list");
    gtk_widget_set_margin_start(list, 24);
    gtk_widget_set_margin_end(list, 24);
    gtk_widget_set_margin_top(list, 24);
    
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), list);
    gtk_widget_set_vexpand(scroll, TRUE);

    gtk_box_append(GTK_BOX(box), empty);
    gtk_box_append(GTK_BOX(box), scroll);

    MergeState *state = g_new0(MergeState, 1);
    state->list_box    = list;
    state->empty_state = empty;
    state->files       = g_ptr_array_new_with_free_func(g_free);

    g_object_set_data_full(G_OBJECT(box), "merge-state", state, merge_state_free);
    update_ui(state);

    return box;
}

/* =========================================================================
 * REGISTRATION
 * ========================================================================= */

const HelvetiaToolCommand pdf_merge_cmds[] = {
    {
        .id        = "add",
        .name      = "Add Files",
        .icon_name = "list-add-symbolic",
        .tooltip   = "Add PDF files",
        .accel     = "<Control>o",
        .activate  = cmd_add
    },
    {
        .id        = "merge",
        .name      = "Merge",
        .icon_name = "document-save-as-symbolic",
        .tooltip   = "Merge and save",
        .accel     = "<Control>s",
        .activate  = cmd_merge
    },
    { NULL }
};
