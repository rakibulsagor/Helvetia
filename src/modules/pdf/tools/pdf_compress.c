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
    GtkWidget *quality_combo;
    GtkWidget *metadata_switch;
    char      *input_path;
    char      *output_path;
} CompressState;

static void compress_state_free(gpointer data) {
    CompressState *state = data;
    g_free(state->input_path);
    g_free(state->output_path);
    g_free(state);
}

static CompressState *get_state(GtkWidget *view) {
    return g_object_get_data(G_OBJECT(view), "compress-state");
}

/* =========================================================================
 * UI HELPERS
 * ========================================================================= */

static void update_ui(CompressState *state) {
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
    CompressState *state = task_data;
    (void)source_object;
    (void)cancellable;

    GError *err = NULL;
    
    int active = adw_combo_row_get_selected(ADW_COMBO_ROW(state->quality_combo));
    const char *quality = "medium";
    if (active == 0) quality = "low";
    else if (active == 2) quality = "high";

    gboolean preserve_metadata = gtk_switch_get_active(GTK_SWITCH(state->metadata_switch));

    gboolean ok = qpdf_compress_file(state->input_path,
                                     state->output_path,
                                     quality,
                                     preserve_metadata,
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
        pdf_show_info(view, "PDF compressed successfully");
    }
}

/* -------------------------------------------------------------------------
 * ADD FILE
 * ------------------------------------------------------------------------- */
static void on_file_chosen(GObject *source, GAsyncResult *res, gpointer user_data) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source);
    GtkWidget *view = user_data;
    CompressState *state = get_state(view);

    GFile *file = gtk_file_dialog_open_finish(dialog, res, NULL);
    if (!file) return;

    g_free(state->input_path);
    state->input_path = g_file_get_path(file);
    g_object_unref(file);

    update_ui(state);
}

static void cmd_add(GtkWidget *view) {
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Select PDF to Compress");
    gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(pdf_filter_store_full()));

    GtkWindow *parent = GTK_WINDOW(gtk_widget_get_root(view));
    gtk_file_dialog_open(dialog, parent, NULL, on_file_chosen, view);
    g_object_unref(dialog);
}

/* -------------------------------------------------------------------------
 * COMPRESS
 * ------------------------------------------------------------------------- */
static void on_save_chosen(GObject *source, GAsyncResult *res, gpointer user_data) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source);
    GtkWidget *view = user_data;
    CompressState *state = get_state(view);

    GFile *file = gtk_file_dialog_save_finish(dialog, res, NULL);
    if (!file) return;

    g_free(state->output_path);
    state->output_path = g_file_get_path(file);
    g_object_unref(file);

    /* Run backend */
    pdf_run_task_async(view, "Compressing PDF…", worker, state, NULL, on_done);
}

static void cmd_compress(GtkWidget *view) {
    CompressState *state = get_state(view);
    if (!state->input_path) {
        pdf_show_error(view, "Select a PDF first.");
        return;
    }

    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Save Compressed PDF");
    gtk_file_dialog_set_initial_name(dialog, "Compressed.pdf");

    GtkWindow *parent = GTK_WINDOW(gtk_widget_get_root(view));
    gtk_file_dialog_save(dialog, parent, NULL, on_save_chosen, view);
    g_object_unref(dialog);
}

/* =========================================================================
 * VIEW LIFECYCLE
 * ========================================================================= */

GtkWidget *pdf_compress_create_view(void) {
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

    GtkWidget *list = gtk_list_box_new();
    gtk_widget_add_css_class(list, "boxed-list");

    GtkWidget *file_row = gtk_list_box_row_new();
    GtkWidget *file_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_start(file_box, 12);
    gtk_widget_set_margin_end(file_box, 12);
    gtk_widget_set_margin_top(file_box, 12);
    gtk_widget_set_margin_bottom(file_box, 12);
    GtkWidget *file_icon = gtk_image_new_from_icon_name("application-pdf-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(file_icon), 32);
    GtkWidget *file_lbl = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(file_lbl), 0.0);
    gtk_widget_set_hexpand(file_lbl, TRUE);
    gtk_box_append(GTK_BOX(file_box), file_icon);
    gtk_box_append(GTK_BOX(file_box), file_lbl);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(file_row), file_box);
    gtk_list_box_append(GTK_LIST_BOX(list), file_row);

    GtkWidget *quality_row = adw_combo_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(quality_row), "Quality");
    GtkStringList *strings = gtk_string_list_new(NULL);
    gtk_string_list_append(strings, "Low (Maximum compression)");
    gtk_string_list_append(strings, "Medium (Recommended)");
    gtk_string_list_append(strings, "High (Better quality)");
    adw_combo_row_set_model(ADW_COMBO_ROW(quality_row), G_LIST_MODEL(strings));
    adw_combo_row_set_selected(ADW_COMBO_ROW(quality_row), 1);
    gtk_list_box_append(GTK_LIST_BOX(list), quality_row);

    GtkWidget *meta_row = adw_action_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(meta_row), "Preserve Metadata");
    GtkWidget *meta_switch = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(meta_switch), TRUE);
    gtk_widget_set_valign(meta_switch, GTK_ALIGN_CENTER);
    adw_action_row_add_suffix(ADW_ACTION_ROW(meta_row), meta_switch);
    gtk_list_box_append(GTK_LIST_BOX(list), meta_row);

    gtk_box_append(GTK_BOX(content), list);

    gtk_box_append(GTK_BOX(box), empty);
    gtk_box_append(GTK_BOX(box), content);

    CompressState *state = g_new0(CompressState, 1);
    state->empty_state     = empty;
    state->content_state   = content;
    state->file_label      = file_lbl;
    state->quality_combo   = quality_row;
    state->metadata_switch = meta_switch;

    g_object_set_data_full(G_OBJECT(box), "compress-state", state, compress_state_free);
    update_ui(state);

    return box;
}

/* =========================================================================
 * REGISTRATION
 * ========================================================================= */

const HelvetiaToolCommand pdf_compress_cmds[] = {
    {
        .id        = "add",
        .name      = "Select File",
        .icon_name = "document-open-symbolic",
        .tooltip   = "Select PDF file",
        .accel     = "<Control>o",
        .activate  = cmd_add
    },
    {
        .id        = "compress",
        .name      = "Compress",
        .icon_name = "view-restore-symbolic",
        .tooltip   = "Compress PDF",
        .accel     = "<Control>s",
        .activate  = cmd_compress
    },
    { NULL }
};
