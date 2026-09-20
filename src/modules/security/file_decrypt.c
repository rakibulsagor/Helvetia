#include "security_module.h"
#include "helv_crypto.h"
#include "../ui/widgets.h"
#include <adwaita.h>
#include <glib/gstdio.h>

typedef struct {
    char *in_path;
    char *out_path;
    char *keyfile_path;
    
    GtkWidget *file_lbl;
    GtkWidget *pass_entry;
    GtkWidget *keyfile_lbl;
    GtkWidget *btn;
    
    GtkWidget *preview_icon;
    GtkWidget *preview_name_lbl;
    GtkWidget *preview_size_lbl;
    GtkWidget *preview_type_lbl;
    GtkWidget *preview_status_lbl;
    GtkWidget *preview_box;
    GtkWidget *preview_empty;
    
    GtkWidget *result_lbl;
} DecryptCtx;

static void set_result_ui(DecryptCtx *ctx, const char *msg) {
    gtk_label_set_text(GTK_LABEL(ctx->result_lbl), msg);
}



static void start_crypto(DecryptCtx *ctx) {
    if (!ctx->in_path) {
        set_result_ui(ctx, "Please choose a file to decrypt.");
        return;
    }
    const char *password = gtk_editable_get_text(GTK_EDITABLE(ctx->pass_entry));
    if (!password || !*password) {
        set_result_ui(ctx, "Please enter the decryption password.");
        return;
    }

    if (g_str_has_suffix(ctx->in_path, ".helv")) {
        ctx->out_path = g_strndup(ctx->in_path, strlen(ctx->in_path) - 5);
    } else {
        ctx->out_path = g_strdup_printf("%s.decrypted", ctx->in_path);
    }

    gtk_widget_set_sensitive(ctx->btn, FALSE);
    set_result_ui(ctx, "Processing... (Argon2id may take a moment)");
    
    // Quick blocking call (as in original code, but could be threaded)
    uint8_t *kf_data = NULL;
    size_t kf_len = 0;
    if (ctx->keyfile_path) {
        g_file_get_contents(ctx->keyfile_path, (gchar **)&kf_data, &kf_len, NULL);
    }
    
    helv_crypto_result_t rc = helv_decrypt_file(ctx->in_path, ctx->out_path, password, kf_data, kf_len, NULL, NULL);
    
    if (kf_data) g_free(kf_data);

    gtk_widget_set_sensitive(ctx->btn, TRUE);
    
    if (rc == DECRYPT_OK) {
        char *msg = g_strdup_printf("✓ Decrypted successfully to %s", ctx->out_path);
        set_result_ui(ctx, msg);
        g_free(msg);
    } else {
        set_result_ui(ctx, helv_crypto_error_msg(rc));
    }
    
    g_free(ctx->out_path);
    ctx->out_path = NULL;
}

static void on_decrypt(GtkButton *btn, gpointer ud) { (void)btn; start_crypto((DecryptCtx*)ud); }

static void on_reset(GtkButton *btn, gpointer ud) {
    (void)btn;
    DecryptCtx *ctx = ud;
    g_free(ctx->in_path); ctx->in_path = NULL;
    g_free(ctx->keyfile_path); ctx->keyfile_path = NULL;
    
    gtk_label_set_text(GTK_LABEL(ctx->file_lbl), "No file selected.");
    gtk_label_set_text(GTK_LABEL(ctx->keyfile_lbl), "No key file selected.");
    gtk_editable_set_text(GTK_EDITABLE(ctx->pass_entry), "");
    gtk_label_set_text(GTK_LABEL(ctx->result_lbl), "");
    gtk_widget_set_visible(ctx->preview_box, FALSE);
    gtk_widget_set_visible(ctx->preview_empty, TRUE);
}

static void update_preview(DecryptCtx *ctx, const char *path) {
    char *basename = g_path_get_basename(path);
    
    FILE *in = g_fopen(path, "rb");
    if (in) {
        helv_header_t h;
        if (helv_read_header(in, &h) == DECRYPT_OK) {
            char *size_str = g_format_size(h.original_size);
            char *sz_txt = g_strdup_printf("Original Size: %s", size_str);
            char *type_txt = g_strdup_printf("Algorithm: %s", h.algorithm == 1 ? "AES-256-GCM" : "ChaCha20-Poly1305");
            
            gtk_widget_set_visible(ctx->preview_empty, FALSE);
            gtk_widget_set_visible(ctx->preview_box, TRUE);
            gtk_image_set_from_icon_name(GTK_IMAGE(ctx->preview_icon), "emblem-readonly");
            
            gtk_label_set_text(GTK_LABEL(ctx->preview_name_lbl), h.filename);
            gtk_label_set_text(GTK_LABEL(ctx->preview_size_lbl), sz_txt);
            gtk_label_set_text(GTK_LABEL(ctx->preview_type_lbl), type_txt);
            gtk_label_set_text(GTK_LABEL(ctx->preview_status_lbl), (h.flags & 1) ? "Requires Key File" : "Ready to decrypt");
            
            g_free(size_str); g_free(sz_txt); g_free(type_txt);
            helv_header_free(&h);
        } else {
            // Fallback for corrupted/invalid files
            gtk_widget_set_visible(ctx->preview_empty, FALSE);
            gtk_widget_set_visible(ctx->preview_box, TRUE);
            gtk_image_set_from_icon_name(GTK_IMAGE(ctx->preview_icon), "dialog-error-symbolic");
            gtk_label_set_text(GTK_LABEL(ctx->preview_name_lbl), basename);
            gtk_label_set_text(GTK_LABEL(ctx->preview_size_lbl), "Size: Unknown");
            gtk_label_set_text(GTK_LABEL(ctx->preview_type_lbl), "Type: Invalid/Corrupt");
            gtk_label_set_text(GTK_LABEL(ctx->preview_status_lbl), "Cannot parse header");
        }
        fclose(in);
    }
    
    g_free(basename);
}

static void on_file_picked(GObject *source, GAsyncResult *res, gpointer data) {
    DecryptCtx *ctx = data;
    GError *err = NULL;
    GFile *f = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source), res, &err);
    if (f) {
        g_free(ctx->in_path);
        ctx->in_path = g_file_get_path(f);
        char *base = g_path_get_basename(ctx->in_path);
        gtk_label_set_text(GTK_LABEL(ctx->file_lbl), base);
        g_free(base);
        update_preview(ctx, ctx->in_path);
        g_object_unref(f);
    }
}

static void on_keyfile_picked(GObject *source, GAsyncResult *res, gpointer data) {
    DecryptCtx *ctx = data;
    GError *err = NULL;
    GFile *f = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source), res, &err);
    if (f) {
        g_free(ctx->keyfile_path);
        ctx->keyfile_path = g_file_get_path(f);
        char *base = g_path_get_basename(ctx->keyfile_path);
        gtk_label_set_text(GTK_LABEL(ctx->keyfile_lbl), base);
        g_free(base);
        g_object_unref(f);
    }
}

static void on_browse(GtkButton *btn, gpointer ud) {
    (void)btn;
    GtkFileDialog *dlg = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dlg, "Choose encrypted file");
    gtk_file_dialog_open(dlg, NULL, NULL, on_file_picked, ud);
}

static void on_browse_keyfile(GtkButton *btn, gpointer ud) {
    (void)btn;
    GtkFileDialog *dlg = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dlg, "Choose key file");
    gtk_file_dialog_open(dlg, NULL, NULL, on_keyfile_picked, ud);
}

static GtkWidget* create_custom_file_row(const char *title, GtkWidget **lbl_out, GCallback browse_cb, gpointer ctx_data) {
    GtkWidget *row = adw_action_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), title);
    
    GtkWidget *val_lbl = gtk_label_new("No file selected.");
    gtk_label_set_ellipsize(GTK_LABEL(val_lbl), PANGO_ELLIPSIZE_MIDDLE);
    gtk_widget_set_halign(val_lbl, GTK_ALIGN_END);
    gtk_widget_set_hexpand(val_lbl, TRUE);
    gtk_widget_add_css_class(val_lbl, "dim-label");
    *lbl_out = val_lbl;
    
    GtkWidget *btn = gtk_button_new_with_label("Browse...");
    gtk_widget_set_valign(btn, GTK_ALIGN_CENTER);
    g_signal_connect(btn, "clicked", browse_cb, ctx_data);
    
    adw_action_row_add_suffix(ADW_ACTION_ROW(row), val_lbl);
    adw_action_row_add_suffix(ADW_ACTION_ROW(row), btn);
    return row;
}

static GtkWidget* create_dec_pane(DecryptCtx *ctx) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_size_request(box, 350, -1);
    
    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(header_box, GTK_ALIGN_CENTER);
    GtkWidget *icon = gtk_image_new_from_icon_name("changes-allow-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 32);
    GtkWidget *title = gtk_label_new("Decrypt File");
    gtk_widget_add_css_class(title, "title-2");
    gtk_box_append(GTK_BOX(header_box), icon);
    gtk_box_append(GTK_BOX(header_box), title);
    
    GtkWidget *subtitle = gtk_label_new("Decrypt previously encrypted Helvetia files.");
    gtk_widget_add_css_class(subtitle, "helvetia-fg-muted");
    gtk_widget_set_halign(subtitle, GTK_ALIGN_CENTER);
    
    gtk_box_append(GTK_BOX(box), header_box);
    gtk_box_append(GTK_BOX(box), subtitle);
    
    GtkWidget *group = adw_preferences_group_new();
    gtk_widget_set_margin_top(group, 8);
    
    GtkWidget *file_row = create_custom_file_row("Encrypted file", &ctx->file_lbl, G_CALLBACK(on_browse), ctx);
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(group), file_row);
    
    GtkWidget *keyfile_row = create_custom_file_row("Key file (Optional)", &ctx->keyfile_lbl, G_CALLBACK(on_browse_keyfile), ctx);
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(group), keyfile_row);
    
    GtkWidget *pass_row = adw_password_entry_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(pass_row), "Password");
    ctx->pass_entry = pass_row;
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(group), pass_row);
    
    gtk_box_append(GTK_BOX(box), group);
    
    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    ctx->btn = gtk_button_new_with_label("Decrypt File");
    gtk_widget_add_css_class(ctx->btn, "suggested-action");
    GtkWidget *reset_btn = gtk_button_new_with_label("Reset");
    gtk_box_append(GTK_BOX(btn_box), ctx->btn);
    gtk_box_append(GTK_BOX(btn_box), reset_btn);
    gtk_box_append(GTK_BOX(box), btn_box);
    
    g_signal_connect(ctx->btn, "clicked", G_CALLBACK(on_decrypt), ctx);
    g_signal_connect(reset_btn, "clicked", G_CALLBACK(on_reset), ctx);
    
    return box;
}

static GtkWidget* create_preview_pane(DecryptCtx *ctx) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_size_request(box, 300, -1);
    gtk_widget_add_css_class(box, "card");
    gtk_widget_set_margin_start(box, 16);
    gtk_widget_set_margin_end(box, 16);
    gtk_widget_set_margin_top(box, 24);
    gtk_widget_set_margin_bottom(box, 24);
    
    GtkWidget *overlay = gtk_overlay_new();
    
    // Empty State
    ctx->preview_empty = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_valign(ctx->preview_empty, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(ctx->preview_empty, GTK_ALIGN_CENTER);
    GtkWidget *e_icon = gtk_image_new_from_icon_name("document-send-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(e_icon), 48);
    gtk_widget_add_css_class(e_icon, "dim-label");
    GtkWidget *e_lbl = gtk_label_new("Select a file\nto inspect");
    gtk_label_set_justify(GTK_LABEL(e_lbl), GTK_JUSTIFY_CENTER);
    gtk_widget_add_css_class(e_lbl, "dim-label");
    gtk_box_append(GTK_BOX(ctx->preview_empty), e_icon);
    gtk_box_append(GTK_BOX(ctx->preview_empty), e_lbl);
    
    // Filled State
    ctx->preview_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_valign(ctx->preview_box, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(ctx->preview_box, GTK_ALIGN_CENTER);
    gtk_widget_set_visible(ctx->preview_box, FALSE);
    
    ctx->preview_icon = gtk_image_new_from_icon_name("text-x-generic");
    gtk_image_set_pixel_size(GTK_IMAGE(ctx->preview_icon), 64);
    ctx->preview_name_lbl = gtk_label_new("");
    gtk_widget_add_css_class(ctx->preview_name_lbl, "title-3");
    gtk_label_set_ellipsize(GTK_LABEL(ctx->preview_name_lbl), PANGO_ELLIPSIZE_MIDDLE);
    gtk_widget_set_size_request(ctx->preview_name_lbl, 250, -1);
    
    ctx->preview_size_lbl = gtk_label_new("");
    ctx->preview_type_lbl = gtk_label_new("");
    ctx->preview_status_lbl = gtk_label_new("");
    gtk_widget_add_css_class(ctx->preview_status_lbl, "accent");
    
    gtk_box_append(GTK_BOX(ctx->preview_box), ctx->preview_icon);
    gtk_box_append(GTK_BOX(ctx->preview_box), ctx->preview_name_lbl);
    gtk_box_append(GTK_BOX(ctx->preview_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(ctx->preview_box), ctx->preview_size_lbl);
    gtk_box_append(GTK_BOX(ctx->preview_box), ctx->preview_type_lbl);
    gtk_box_append(GTK_BOX(ctx->preview_box), ctx->preview_status_lbl);
    
    gtk_overlay_set_child(GTK_OVERLAY(overlay), ctx->preview_empty);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), ctx->preview_box);
    
    gtk_box_append(GTK_BOX(box), overlay);
    return box;
}

GtkWidget *build_file_decrypt(void) {
    DecryptCtx *ctx = g_new0(DecryptCtx, 1);
    
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(main_box, 24);
    gtk_widget_set_margin_end(main_box, 24);
    gtk_widget_set_margin_top(main_box, 24);
    gtk_widget_set_margin_bottom(main_box, 24);
    
    GtkWidget *flow = gtk_flow_box_new();
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flow), GTK_SELECTION_NONE);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flow), 2);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(flow), 1);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(flow), FALSE);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(flow), 32);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(flow), 32);
    gtk_widget_set_halign(flow, GTK_ALIGN_CENTER);
    
    GtkWidget *dec_pane = create_dec_pane(ctx);
    GtkWidget *preview_pane = create_preview_pane(ctx);
    
    gtk_widget_set_hexpand(dec_pane, TRUE);
    gtk_widget_set_hexpand(preview_pane, TRUE);
    
    gtk_flow_box_append(GTK_FLOW_BOX(flow), dec_pane);
    gtk_flow_box_append(GTK_FLOW_BOX(flow), preview_pane);
    
    gtk_box_append(GTK_BOX(main_box), flow);
    
    GtkWidget *status_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_top(status_box, 32);
    gtk_widget_set_halign(status_box, GTK_ALIGN_CENTER);
    
    ctx->result_lbl = gtk_label_new("");
    gtk_widget_add_css_class(ctx->result_lbl, "title-4");
    gtk_box_append(GTK_BOX(status_box), ctx->result_lbl);
    
    gtk_box_append(GTK_BOX(main_box), status_box);
    
    g_signal_connect_swapped(main_box, "destroy", G_CALLBACK(g_free), ctx);
    return main_box;
}
