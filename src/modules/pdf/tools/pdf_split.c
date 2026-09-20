#include "../pdf_tools.h"
#include "../pdf_shared.h"
#include "../backend/qpdf_wrapper.h"

/* =========================================================================
 * STATE
 * ========================================================================= */

typedef struct {
    GtkWidget *empty_state;
    GtkWidget *content_state;
    GtkWidget *file_label;
    GtkWidget *ranges_entry;
    char      *input_path;
    char      *output_dir;
} SplitState;

static void split_state_free(gpointer data) {
    SplitState *state = data;
    g_free(state->input_path);
    g_free(state->output_dir);
    g_free(state);
}

static SplitState *get_state(GtkWidget *view) {
    return g_object_get_data(G_OBJECT(view), "split-state");
}

/* =========================================================================
 * UI HELPERS
 * ========================================================================= */

static void update_ui(SplitState *state) {
    gboolean has_file = (state->input_path != NULL);
    gtk_widget_set_visible(state->empty_state, !has_file);
    gtk_widget_set_visible(state->content_state, has_file);

    if (has_file) {
        char *stem = pdf_stem(state->input_path);
        gtk_label_set_text(GTK_LABEL(state->file_label), stem);
        g_free(stem);
    }
}

/* =========================================================================
 * COMMANDS & WORKERS
 * ========================================================================= */

static void worker(GTask *task, gpointer source_object,
                   gpointer task_data, GCancellable *cancellable) {
    SplitState *state = task_data;
    (void)source_object;
    (void)cancellable;

    GError *err = NULL;
    const char *ranges = gtk_editable_get_text(GTK_EDITABLE(state->ranges_entry));
    if (!ranges || *ranges == '\0') ranges = "1-z"; // Default to all

    gboolean ok = qpdf_split_file(state->input_path,
                                  state->output_dir,
                                  ranges,
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
        pdf_show_info(view, "PDF split successfully");
    }
}

/* -------------------------------------------------------------------------
 * ADD FILE
 * ------------------------------------------------------------------------- */
static void on_file_chosen(GObject *source, GAsyncResult *res, gpointer user_data) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source);
    GtkWidget *view = user_data;
    SplitState *state = get_state(view);

    GFile *file = gtk_file_dialog_open_finish(dialog, res, NULL);
    if (!file) return;

    g_free(state->input_path);
    state->input_path = g_file_get_path(file);
    g_object_unref(file);

    update_ui(state);
}

static void cmd_add(GtkWidget *view) {
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Select PDF to Split");
    gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(pdf_filter_store_full()));

    GtkWindow *parent = GTK_WINDOW(gtk_widget_get_root(view));
    gtk_file_dialog_open(dialog, parent, NULL, on_file_chosen, view);
    g_object_unref(dialog);
}

/* -------------------------------------------------------------------------
 * SPLIT
 * ------------------------------------------------------------------------- */
static void on_dir_chosen(GObject *source, GAsyncResult *res, gpointer user_data) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source);
    GtkWidget *view = user_data;
    SplitState *state = get_state(view);

    GFile *dir = gtk_file_dialog_select_folder_finish(dialog, res, NULL);
    if (!dir) return;

    g_free(state->output_dir);
    state->output_dir = g_file_get_path(dir);
    g_object_unref(dir);

    /* Run backend */
    pdf_run_task_async(view, "Splitting PDF…", worker, state, NULL, on_done);
}

static void cmd_split(GtkWidget *view) {
    SplitState *state = get_state(view);
    if (!state->input_path) {
        pdf_show_error(view, "Select a PDF first.");
        return;
    }

    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Select Output Folder");

    GtkWindow *parent = GTK_WINDOW(gtk_widget_get_root(view));
    gtk_file_dialog_select_folder(dialog, parent, NULL, on_dir_chosen, view);
    g_object_unref(dialog);
}

/* =========================================================================
 * VIEW LIFECYCLE
 * ========================================================================= */

GtkWidget *pdf_split_create_view(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    /* Empty state */
    GtkWidget *empty = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_valign(empty, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(empty, GTK_ALIGN_CENTER);
    gtk_widget_set_vexpand(empty, TRUE);

    GtkWidget *img = gtk_image_new_from_icon_name("application-pdf-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(img), 64);
    GtkWidget *lbl = gtk_label_new("No PDF Selected");
    gtk_widget_add_css_class(lbl, "title-2");

    gtk_box_append(GTK_BOX(empty), img);
    gtk_box_append(GTK_BOX(empty), lbl);

    /* Content state */
    GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start(content, 24);
    gtk_widget_set_margin_end(content, 24);
    gtk_widget_set_margin_top(content, 24);

    GtkWidget *file_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *file_icon = gtk_image_new_from_icon_name("application-pdf-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(file_icon), 32);
    GtkWidget *file_lbl = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(file_lbl), 0.0);
    gtk_widget_set_hexpand(file_lbl, TRUE);
    gtk_box_append(GTK_BOX(file_box), file_icon);
    gtk_box_append(GTK_BOX(file_box), file_lbl);

    GtkWidget *ranges_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *ranges_lbl = gtk_label_new("Ranges:");
    GtkWidget *ranges_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(ranges_entry), "e.g. 1-3, 5, 7-z");
    gtk_widget_set_hexpand(ranges_entry, TRUE);
    gtk_box_append(GTK_BOX(ranges_box), ranges_lbl);
    gtk_box_append(GTK_BOX(ranges_box), ranges_entry);

    gtk_box_append(GTK_BOX(content), file_box);
    gtk_box_append(GTK_BOX(content), ranges_box);

    gtk_box_append(GTK_BOX(box), empty);
    gtk_box_append(GTK_BOX(box), content);

    SplitState *state = g_new0(SplitState, 1);
    state->empty_state   = empty;
    state->content_state = content;
    state->file_label    = file_lbl;
    state->ranges_entry  = ranges_entry;

    g_object_set_data_full(G_OBJECT(box), "split-state", state, split_state_free);
    update_ui(state);

    return box;
}

/* =========================================================================
 * REGISTRATION
 * ========================================================================= */

const HelvetiaToolCommand pdf_split_cmds[] = {
    {
        .id        = "add",
        .name      = "Select File",
        .icon_name = "document-open-symbolic",
        .tooltip   = "Select PDF file",
        .accel     = "<Control>o",
        .activate  = cmd_add
    },
    {
        .id        = "split",
        .name      = "Split",
        .icon_name = "edit-cut-symbolic",
        .tooltip   = "Split PDF",
        .accel     = "<Control>s",
        .activate  = cmd_split
    },
    { NULL }
};
