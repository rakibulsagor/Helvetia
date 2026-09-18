/* ================================================================
 * Helvetia — Shared Widget Helpers Implementation
 * ================================================================ */
#include "widgets.h"
#include <string.h>

/* -------------------------------------------------------------- */
GtkWidget *hv_make_result_label(void) {
    GtkWidget *lbl = gtk_label_new("—");
    gtk_widget_add_css_class(lbl, "helvetia-result");
    gtk_label_set_selectable(GTK_LABEL(lbl), TRUE);
    gtk_label_set_wrap(GTK_LABEL(lbl), TRUE);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
    return lbl;
}

/* -------------------------------------------------------------- */
GtkWidget *hv_make_entry_row(const char *label, GtkWidget **entry_out) {
    GtkWidget *box   = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *lbl   = gtk_label_new(label);
    GtkWidget *entry = gtk_entry_new();
    gtk_widget_set_size_request(lbl, 120, -1);
    gtk_widget_set_halign(lbl, GTK_ALIGN_END);
    gtk_widget_set_hexpand(entry, TRUE);
    gtk_box_append(GTK_BOX(box), lbl);
    gtk_box_append(GTK_BOX(box), entry);
    if (entry_out) *entry_out = entry;
    return box;
}

/* -------------------------------------------------------------- */
static void on_copy_clicked(GtkButton *btn, gpointer label) {
    (void)btn;
    const char *text = gtk_label_get_text(GTK_LABEL(label));
    if (!text || !*text || !strcmp(text, "—")) return;
    GdkClipboard *cb = gdk_display_get_clipboard(gdk_display_get_default());
    gdk_clipboard_set_text(cb, text);
}

GtkWidget *hv_make_copy_btn(GtkWidget *source_label) {
    GtkWidget *btn = gtk_button_new_from_icon_name("edit-copy-symbolic");
    gtk_widget_set_tooltip_text(btn, "Copy to clipboard");
    g_signal_connect(btn, "clicked", G_CALLBACK(on_copy_clicked), source_label);
    return btn;
}

/* -------------------------------------------------------------- */
GtkWidget *hv_make_action_btn(const char *label) {
    GtkWidget *btn = gtk_button_new_with_label(label);
    gtk_widget_add_css_class(btn, "suggested-action");
    return btn;
}

/* -------------------------------------------------------------- */
GtkWidget *hv_make_text_view(GtkWidget **textview_out, gboolean editable) {
    GtkWidget *sw = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(sw, TRUE);
    gtk_widget_set_size_request(sw, -1, 140);

    GtkWidget *tv = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(tv), editable);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(tv), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(tv), TRUE);
    gtk_widget_set_margin_start(tv, 6);
    gtk_widget_set_margin_end(tv, 6);
    gtk_widget_set_margin_top(tv, 6);
    gtk_widget_set_margin_bottom(tv, 6);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sw), tv);
    if (textview_out) *textview_out = tv;
    return sw;
}

/* -------------------------------------------------------------- */
void hv_textview_set_text(GtkWidget *tv, const char *text) {
    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv));
    gtk_text_buffer_set_text(buf, text ? text : "", -1);
}

char *hv_textview_get_text(GtkWidget *tv) {
    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv));
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buf, &start, &end);
    return gtk_text_buffer_get_text(buf, &start, &end, FALSE);
}

/* -------------------------------------------------------------- */
GtkWidget *hv_make_separator(void) {
    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_margin_top(sep, 4);
    gtk_widget_set_margin_bottom(sep, 4);
    return sep;
}

/* -------------------------------------------------------------- */
char *hv_run_cmd(const char *cmd) {
    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;
    GString *out = g_string_new(NULL);
    char buf[4096];
    while (fgets(buf, sizeof buf, fp))
        g_string_append(out, buf);
    pclose(fp);
    return g_string_free(out, FALSE);
}

/* -------------------------------------------------------------- */
static void on_file_opened(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);
    GtkWidget *entry = GTK_WIDGET(user_data);
    GError *err = NULL;
    GFile *file = gtk_file_dialog_open_finish(dialog, res, &err);
    if (file) {
        char *path = g_file_get_path(file);
        if (path) {
            gtk_editable_set_text(GTK_EDITABLE(entry), path);
            g_free(path);
        }
        g_object_unref(file);
    } else {
        if (err) g_error_free(err);
    }
}

static void on_file_saved(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);
    GtkWidget *entry = GTK_WIDGET(user_data);
    GError *err = NULL;
    GFile *file = gtk_file_dialog_save_finish(dialog, res, &err);
    if (file) {
        char *path = g_file_get_path(file);
        if (path) {
            gtk_editable_set_text(GTK_EDITABLE(entry), path);
            g_free(path);
        }
        g_object_unref(file);
    } else {
        if (err) g_error_free(err);
    }
}

static void on_browse_clicked(GtkButton *btn, gpointer user_data) {
    GtkWidget *entry = GTK_WIDGET(user_data);
    gboolean is_save = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "is_save"));
    
    GtkFileDialog *dialog = gtk_file_dialog_new();
    GtkRoot *root = gtk_widget_get_root(GTK_WIDGET(btn));
    GtkWindow *win = GTK_IS_WINDOW(root) ? GTK_WINDOW(root) : NULL;
    
    if (is_save) {
        gtk_file_dialog_save(dialog, win, NULL, on_file_saved, entry);
    } else {
        gtk_file_dialog_open(dialog, win, NULL, on_file_opened, entry);
    }
}

GtkWidget *hv_make_file_picker_row(const char *label, GtkWidget **entry_out, gboolean is_save) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *lbl = gtk_label_new(label);
    gtk_widget_set_size_request(lbl, 120, -1);
    gtk_widget_set_halign(lbl, GTK_ALIGN_END);
    
    GtkWidget *entry = gtk_entry_new();
    gtk_widget_set_hexpand(entry, TRUE);
    if (entry_out) *entry_out = entry;
    
    GtkWidget *btn = gtk_button_new_with_label("Browse...");
    g_object_set_data(G_OBJECT(btn), "is_save", GINT_TO_POINTER(is_save));
    g_signal_connect(btn, "clicked", G_CALLBACK(on_browse_clicked), entry);
    
    gtk_box_append(GTK_BOX(box), lbl);
    gtk_box_append(GTK_BOX(box), entry);
    gtk_box_append(GTK_BOX(box), btn);
    return box;
}
