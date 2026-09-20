#include "security_module.h"
#include "helv_crypto.h"
#include "../ui/widgets.h"
#include <adwaita.h>
#include <glib/gstdio.h>
#include <string.h>

typedef struct {
    char *folder_path;
    char *keyfile_path;
    
    GtkWidget *folder_lbl;
    GtkWidget *keyfile_lbl;
    GtkWidget *pass_entry;
    GtkWidget *alg_combo;
    GtkWidget *comp_combo;
    GtkWidget *btn;
    
    // Options
    GtkWidget *chk_hidden;
    GtkWidget *chk_perms;
    GtkWidget *chk_owner;
    GtkWidget *chk_times;
    GtkWidget *chk_symlinks;
    GtkWidget *chk_shred;

    GtkWidget *preview_icon;
    GtkWidget *preview_name_lbl;
    GtkWidget *preview_status_lbl;
    GtkWidget *preview_box;
    GtkWidget *preview_empty;
    
    GtkWidget *result_lbl;
} FolderEncryptCtx;

static void set_result_ui(FolderEncryptCtx *ctx, const char *msg) {
    gtk_label_set_text(GTK_LABEL(ctx->result_lbl), msg);
}

static void start_crypto(FolderEncryptCtx *ctx) {
    if (!ctx->folder_path) {
        set_result_ui(ctx, "Please choose a folder to encrypt.");
        return;
    }
    
    const char *password = gtk_editable_get_text(GTK_EDITABLE(ctx->pass_entry));
    if (!password || !*password) {
        set_result_ui(ctx, "Please enter an encryption password.");
        return;
    }
    
    int alg = gtk_drop_down_get_selected(GTK_DROP_DOWN(ctx->alg_combo));
    uint8_t alg_id = alg == 0 ? 0x01 : 0x02; // AES256GCM or ChaCha20Poly1305
    
    int comp = gtk_drop_down_get_selected(GTK_DROP_DOWN(ctx->comp_combo));
    uint8_t comp_id = HELV_COMP_NONE;
    if (comp == 1) comp_id = HELV_COMP_ZSTD;
    else if (comp == 2) comp_id = HELV_COMP_GZIP;
    else if (comp == 3) comp_id = HELV_COMP_XZ;

    char *out_path = g_strdup_printf("%s.helv", ctx->folder_path);
    
    gtk_widget_set_sensitive(ctx->btn, FALSE);
    set_result_ui(ctx, "Processing... (Depending on folder size, this may take time)");
    
    uint8_t *kf_data = NULL;
    size_t kf_len = 0;
    if (ctx->keyfile_path) {
        g_file_get_contents(ctx->keyfile_path, (gchar **)&kf_data, &kf_len, NULL);
    }
    
    helv_folder_options_t opts = {0};
    opts.include_hidden = gtk_check_button_get_active(GTK_CHECK_BUTTON(ctx->chk_hidden));
    opts.preserve_permissions = gtk_check_button_get_active(GTK_CHECK_BUTTON(ctx->chk_perms));
    opts.preserve_ownership = gtk_check_button_get_active(GTK_CHECK_BUTTON(ctx->chk_owner));
    opts.preserve_timestamps = gtk_check_button_get_active(GTK_CHECK_BUTTON(ctx->chk_times));
    opts.follow_symlinks = gtk_check_button_get_active(GTK_CHECK_BUTTON(ctx->chk_symlinks));
    opts.shred_original = gtk_check_button_get_active(GTK_CHECK_BUTTON(ctx->chk_shred));
    opts.compression = comp_id;
    opts.chunk_size = HELV_DEFAULT_CHUNK_SIZE;

    helv_crypto_result_t rc = helv_encrypt_folder(ctx->folder_path, out_path, password, kf_data, kf_len, alg_id, &opts, NULL, NULL);
    
    if (kf_data) g_free(kf_data);
    
    gtk_widget_set_sensitive(ctx->btn, TRUE);
    
    if (rc == DECRYPT_OK) {
        char *success_msg = g_strdup_printf("✓ Folder encrypted successfully to %s", out_path);
        set_result_ui(ctx, success_msg);
        g_free(success_msg);
    } else {
        set_result_ui(ctx, helv_crypto_error_msg(rc));
    }
    
    g_free(out_path);
}

static void on_encrypt(GtkButton *btn, gpointer ud) { (void)btn; start_crypto((FolderEncryptCtx*)ud); }

static void on_reset_enc(GtkButton *btn, gpointer ud) {
    (void)btn;
    FolderEncryptCtx *ctx = ud;
    g_free(ctx->folder_path); ctx->folder_path = NULL;
    g_free(ctx->keyfile_path); ctx->keyfile_path = NULL;
    
    gtk_label_set_text(GTK_LABEL(ctx->folder_lbl), "No folder selected.");
    if (ctx->keyfile_lbl) gtk_label_set_text(GTK_LABEL(ctx->keyfile_lbl), "No key file selected.");
    gtk_editable_set_text(GTK_EDITABLE(ctx->pass_entry), "");
    gtk_label_set_text(GTK_LABEL(ctx->result_lbl), "");
    gtk_widget_set_visible(ctx->preview_box, FALSE);
    gtk_widget_set_visible(ctx->preview_empty, TRUE);
}

static void update_preview(FolderEncryptCtx *ctx, const char *path) {
    char *basename = g_path_get_basename(path);
    
    gtk_widget_set_visible(ctx->preview_empty, FALSE);
    gtk_widget_set_visible(ctx->preview_box, TRUE);
    gtk_image_set_from_icon_name(GTK_IMAGE(ctx->preview_icon), "folder-symbolic");
    gtk_label_set_text(GTK_LABEL(ctx->preview_name_lbl), basename);
    gtk_label_set_text(GTK_LABEL(ctx->preview_status_lbl), "Ready to archive and encrypt");
    
    g_free(basename);
}

static void on_folder_picked(GObject *source, GAsyncResult *res, gpointer data) {
    FolderEncryptCtx *ctx = data;
    GError *err = NULL;
    GFile *f = gtk_file_dialog_select_folder_finish(GTK_FILE_DIALOG(source), res, &err);
    if (f) {
        g_free(ctx->folder_path);
        ctx->folder_path = g_file_get_path(f);
        char *base = g_path_get_basename(ctx->folder_path);
        gtk_label_set_text(GTK_LABEL(ctx->folder_lbl), base);
        g_free(base);
        update_preview(ctx, ctx->folder_path);
        g_object_unref(f);
    }
}

static void on_keyfile_picked_enc(GObject *source, GAsyncResult *res, gpointer data) {
    FolderEncryptCtx *ctx = data;
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

static void on_browse_folder(GtkButton *btn, gpointer ud) {
    (void)btn;
    GtkFileDialog *dlg = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dlg, "Choose folder to encrypt");
    gtk_file_dialog_select_folder(dlg, NULL, NULL, on_folder_picked, ud);
}

static void on_browse_keyfile_enc(GtkButton *btn, gpointer ud) {
    (void)btn;
    GtkFileDialog *dlg = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dlg, "Choose key file");
    gtk_file_dialog_open(dlg, NULL, NULL, on_keyfile_picked_enc, ud);
}

static GtkWidget* create_custom_row(const char *title, GtkWidget **lbl_out, GCallback browse_cb, gpointer ctx_data) {
    GtkWidget *row = adw_action_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), title);
    
    GtkWidget *val_lbl = gtk_label_new("None selected.");
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

static GtkWidget* create_check_row(const char *title, gboolean active, GtkWidget **chk_out) {
    GtkWidget *row = adw_action_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), title);
    GtkWidget *chk = gtk_check_button_new();
    gtk_check_button_set_active(GTK_CHECK_BUTTON(chk), active);
    adw_action_row_add_prefix(ADW_ACTION_ROW(row), chk);
    adw_action_row_set_activatable_widget(ADW_ACTION_ROW(row), chk);
    if (chk_out) *chk_out = chk;
    return row;
}

static GtkWidget* create_enc_pane(FolderEncryptCtx *ctx) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_size_request(box, 400, -1);
    
    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(header_box, GTK_ALIGN_CENTER);
    GtkWidget *icon = gtk_image_new_from_icon_name("folder-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 32);
    GtkWidget *title = gtk_label_new("Encrypt Folder");
    gtk_widget_add_css_class(title, "title-2");
    gtk_box_append(GTK_BOX(header_box), icon);
    gtk_box_append(GTK_BOX(header_box), title);
    
    GtkWidget *subtitle = gtk_label_new("Package and encrypt an entire directory tree into a single .helv file.");
    gtk_label_set_wrap(GTK_LABEL(subtitle), TRUE);
    gtk_widget_add_css_class(subtitle, "helvetia-fg-muted");
    gtk_widget_set_halign(subtitle, GTK_ALIGN_CENTER);
    
    gtk_box_append(GTK_BOX(box), header_box);
    gtk_box_append(GTK_BOX(box), subtitle);
    
    GtkWidget *group = adw_preferences_group_new();
    gtk_widget_set_margin_top(group, 8);
    
    GtkWidget *file_row = create_custom_row("Choose folder", &ctx->folder_lbl, G_CALLBACK(on_browse_folder), ctx);
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(group), file_row);
    
    GtkWidget *keyfile_row = create_custom_row("Key file (Optional)", &ctx->keyfile_lbl, G_CALLBACK(on_browse_keyfile_enc), ctx);
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(group), keyfile_row);
    
    GtkWidget *pass_row = adw_password_entry_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(pass_row), "Password");
    ctx->pass_entry = pass_row;
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(group), pass_row);
    
    const char *algs[] = { "AES-256-GCM", "ChaCha20-Poly1305", NULL };
    ctx->alg_combo = gtk_drop_down_new_from_strings(algs);
    gtk_widget_set_valign(ctx->alg_combo, GTK_ALIGN_CENTER);
    GtkWidget *alg_row = adw_action_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(alg_row), "Algorithm");
    adw_action_row_add_suffix(ADW_ACTION_ROW(alg_row), ctx->alg_combo);
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(group), alg_row);

    const char *comps[] = { "None", "zstd", "gzip", "xz", NULL };
    ctx->comp_combo = gtk_drop_down_new_from_strings(comps);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(ctx->comp_combo), 1); // default zstd
    gtk_widget_set_valign(ctx->comp_combo, GTK_ALIGN_CENTER);
    GtkWidget *comp_row = adw_action_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(comp_row), "Compression");
    adw_action_row_add_suffix(ADW_ACTION_ROW(comp_row), ctx->comp_combo);
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(group), comp_row);

    gtk_box_append(GTK_BOX(box), group);
    
    GtkWidget *expander = adw_expander_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(expander), "Advanced Options");
    
    adw_expander_row_add_row(ADW_EXPANDER_ROW(expander), create_check_row("Include hidden files", TRUE, &ctx->chk_hidden));
    adw_expander_row_add_row(ADW_EXPANDER_ROW(expander), create_check_row("Preserve permissions", TRUE, &ctx->chk_perms));
    adw_expander_row_add_row(ADW_EXPANDER_ROW(expander), create_check_row("Preserve ownership", FALSE, &ctx->chk_owner));
    adw_expander_row_add_row(ADW_EXPANDER_ROW(expander), create_check_row("Preserve timestamps", TRUE, &ctx->chk_times));
    adw_expander_row_add_row(ADW_EXPANDER_ROW(expander), create_check_row("Follow symlinks", FALSE, &ctx->chk_symlinks));
    adw_expander_row_add_row(ADW_EXPANDER_ROW(expander), create_check_row("Shred original (Dangerous)", FALSE, &ctx->chk_shred));
    
    GtkWidget *opt_group = adw_preferences_group_new();
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(opt_group), expander);
    gtk_box_append(GTK_BOX(box), opt_group);

    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    ctx->btn = gtk_button_new_with_label("Encrypt Folder");
    gtk_widget_add_css_class(ctx->btn, "suggested-action");
    GtkWidget *reset_btn = gtk_button_new_with_label("Reset");
    gtk_box_append(GTK_BOX(btn_box), ctx->btn);
    gtk_box_append(GTK_BOX(btn_box), reset_btn);
    gtk_box_append(GTK_BOX(box), btn_box);
    
    g_signal_connect(ctx->btn, "clicked", G_CALLBACK(on_encrypt), ctx);
    g_signal_connect(reset_btn, "clicked", G_CALLBACK(on_reset_enc), ctx);
    
    return box;
}

static GtkWidget* create_preview_pane(FolderEncryptCtx *ctx) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_size_request(box, 300, -1);
    gtk_widget_add_css_class(box, "card");
    gtk_widget_set_margin_start(box, 16);
    gtk_widget_set_margin_end(box, 16);
    gtk_widget_set_margin_top(box, 24);
    gtk_widget_set_margin_bottom(box, 24);
    
    GtkWidget *overlay = gtk_overlay_new();
    
    ctx->preview_empty = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_valign(ctx->preview_empty, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(ctx->preview_empty, GTK_ALIGN_CENTER);
    GtkWidget *e_icon = gtk_image_new_from_icon_name("folder-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(e_icon), 48);
    gtk_widget_add_css_class(e_icon, "dim-label");
    GtkWidget *e_lbl = gtk_label_new("Select a folder\nto archive");
    gtk_label_set_justify(GTK_LABEL(e_lbl), GTK_JUSTIFY_CENTER);
    gtk_widget_add_css_class(e_lbl, "dim-label");
    gtk_box_append(GTK_BOX(ctx->preview_empty), e_icon);
    gtk_box_append(GTK_BOX(ctx->preview_empty), e_lbl);
    
    ctx->preview_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_valign(ctx->preview_box, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(ctx->preview_box, GTK_ALIGN_CENTER);
    gtk_widget_set_visible(ctx->preview_box, FALSE);
    
    ctx->preview_icon = gtk_image_new_from_icon_name("folder-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(ctx->preview_icon), 64);
    ctx->preview_name_lbl = gtk_label_new("");
    gtk_widget_add_css_class(ctx->preview_name_lbl, "title-3");
    gtk_label_set_ellipsize(GTK_LABEL(ctx->preview_name_lbl), PANGO_ELLIPSIZE_MIDDLE);
    gtk_widget_set_size_request(ctx->preview_name_lbl, 250, -1);
    
    ctx->preview_status_lbl = gtk_label_new("");
    gtk_widget_add_css_class(ctx->preview_status_lbl, "accent");
    
    gtk_box_append(GTK_BOX(ctx->preview_box), ctx->preview_icon);
    gtk_box_append(GTK_BOX(ctx->preview_box), ctx->preview_name_lbl);
    gtk_box_append(GTK_BOX(ctx->preview_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(ctx->preview_box), ctx->preview_status_lbl);
    
    gtk_overlay_set_child(GTK_OVERLAY(overlay), ctx->preview_empty);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), ctx->preview_box);
    
    gtk_box_append(GTK_BOX(box), overlay);
    return box;
}

GtkWidget *build_folder_encrypt(void) {
    FolderEncryptCtx *ctx = g_new0(FolderEncryptCtx, 1);
    
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
    
    GtkWidget *enc_pane = create_enc_pane(ctx);
    GtkWidget *preview_pane = create_preview_pane(ctx);
    
    gtk_widget_set_hexpand(enc_pane, TRUE);
    gtk_widget_set_hexpand(preview_pane, TRUE);
    
    gtk_flow_box_append(GTK_FLOW_BOX(flow), enc_pane);
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
