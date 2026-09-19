#include "security_module.h"
#include "../ui/widgets.h"
#include <sodium.h>
#include <adwaita.h>
#include <glib/gstdio.h>
#include <string.h>
#include <stdio.h>

#define CHUNK_SIZE 65536
#define HELV_MAGIC "HELV"
#define HELV_VERSION 1
#define ALG_AES256GCM 1
#define ALG_CHACHA20 2
#define KDF_ARGON2ID 1

#pragma pack(push, 1)
typedef struct {
    char magic[4];
    uint8_t version;
    uint8_t alg_id;
    uint8_t kdf_id;
    uint32_t mem_cost;
    uint32_t time_cost;
    uint32_t parallel;
    uint32_t reserved;
    uint8_t salt[16];
    uint8_t nonce[12];
    uint16_t name_len;
} HelvHeader;
#pragma pack(pop)

typedef struct {
    char *enc_in_path;
    char *dec_in_path;
    
    GtkWidget *enc_file_lbl;
    GtkWidget *enc_pass_entry;
    GtkWidget *enc_alg_combo;
    GtkWidget *enc_btn;
    
    GtkWidget *dec_file_lbl;
    GtkWidget *dec_pass_entry;
    GtkWidget *dec_btn;
    
    GtkWidget *preview_icon;
    GtkWidget *preview_name_lbl;
    GtkWidget *preview_size_lbl;
    GtkWidget *preview_type_lbl;
    GtkWidget *preview_status_lbl;
    GtkWidget *preview_box;
    GtkWidget *preview_empty;
    
    GtkWidget *result_lbl;
    
    // Worker state
    char *in_path;
    char *out_path;
    char *password;
    int algorithm;
    gboolean is_encrypt;
} CryptoCtx;

static void set_result_ui(CryptoCtx *ctx, const char *msg) {
    gtk_label_set_text(GTK_LABEL(ctx->result_lbl), msg);
}

static gpointer crypto_worker(gpointer data) {
    CryptoCtx *ctx = data;
    FILE *in = g_fopen(ctx->in_path, "rb");
    if (!in) return NULL;
    
    FILE *out = g_fopen(ctx->out_path, "wb");
    if (!out) { fclose(in); return NULL; }

    if (ctx->is_encrypt) {
        HelvHeader hdr = {0};
        memcpy(hdr.magic, HELV_MAGIC, 4);
        hdr.version = HELV_VERSION;
        hdr.alg_id = ctx->algorithm == 0 ? ALG_AES256GCM : ALG_CHACHA20;
        hdr.kdf_id = KDF_ARGON2ID;
        hdr.mem_cost = 268435456;
        hdr.time_cost = 3;
        hdr.parallel = 4;
        
        randombytes_buf(hdr.salt, sizeof(hdr.salt));
        randombytes_buf(hdr.nonce, sizeof(hdr.nonce));
        
        uint8_t key[32];
        if (crypto_pwhash(key, sizeof(key), ctx->password, strlen(ctx->password),
                          hdr.salt, hdr.time_cost, hdr.mem_cost, crypto_pwhash_ALG_ARGON2ID13) != 0) {
            fclose(in); fclose(out); return NULL;
        }

        fwrite(&hdr, 1, sizeof(hdr), out);

        uint8_t in_buf[CHUNK_SIZE];
        uint8_t out_buf[CHUNK_SIZE + 16];
        size_t bytes_read;
        
        while ((bytes_read = fread(in_buf, 1, sizeof(in_buf), in)) > 0) {
            unsigned long long out_len;
            if (hdr.alg_id == ALG_AES256GCM) {
                crypto_aead_aes256gcm_encrypt(out_buf, &out_len, in_buf, bytes_read, 
                                              (unsigned char*)&hdr, sizeof(hdr), NULL, hdr.nonce, key);
            } else {
                crypto_aead_chacha20poly1305_ietf_encrypt(out_buf, &out_len, in_buf, bytes_read, 
                                              (unsigned char*)&hdr, sizeof(hdr), NULL, hdr.nonce, key);
            }
            fwrite(out_buf, 1, out_len, out);
            hdr.nonce[11]++;
        }
        sodium_memzero(key, sizeof(key));
    } else {
        HelvHeader hdr;
        if (fread(&hdr, 1, sizeof(hdr), in) != sizeof(hdr) || memcmp(hdr.magic, HELV_MAGIC, 4) != 0) {
            fclose(in); fclose(out); return NULL;
        }
        
        uint8_t key[32];
        if (crypto_pwhash(key, sizeof(key), ctx->password, strlen(ctx->password),
                          hdr.salt, hdr.time_cost, hdr.mem_cost, crypto_pwhash_ALG_ARGON2ID13) != 0) {
            fclose(in); fclose(out); return NULL;
        }

        uint8_t in_buf[CHUNK_SIZE + 16];
        uint8_t out_buf[CHUNK_SIZE];
        size_t bytes_read;
        
        while ((bytes_read = fread(in_buf, 1, sizeof(in_buf), in)) > 0) {
            unsigned long long out_len;
            int res = -1;
            if (hdr.alg_id == ALG_AES256GCM) {
                res = crypto_aead_aes256gcm_decrypt(out_buf, &out_len, NULL, in_buf, bytes_read, 
                                                    (unsigned char*)&hdr, sizeof(hdr), hdr.nonce, key);
            } else {
                res = crypto_aead_chacha20poly1305_ietf_decrypt(out_buf, &out_len, NULL, in_buf, bytes_read, 
                                                    (unsigned char*)&hdr, sizeof(hdr), hdr.nonce, key);
            }
            if (res != 0) {
                fclose(in); fclose(out);
                sodium_memzero(key, sizeof(key));
                remove(ctx->out_path);
                return NULL;
            }
            fwrite(out_buf, 1, out_len, out);
            hdr.nonce[11]++;
        }
        sodium_memzero(key, sizeof(key));
    }

    fclose(in); fclose(out);
    return NULL;
}

static void start_crypto(CryptoCtx *ctx, gboolean is_encrypt) {
    if (sodium_init() < 0) {
        set_result_ui(ctx, "Error initializing libsodium.");
        return;
    }
    ctx->is_encrypt = is_encrypt;
    
    if (is_encrypt) {
        if (!ctx->enc_in_path) { set_result_ui(ctx, "Please choose a file to encrypt."); return; }
        ctx->in_path = g_strdup(ctx->enc_in_path);
        ctx->password = g_strdup(gtk_editable_get_text(GTK_EDITABLE(ctx->enc_pass_entry)));
        ctx->algorithm = gtk_drop_down_get_selected(GTK_DROP_DOWN(ctx->enc_alg_combo));
        ctx->out_path = g_strdup_printf("%s.helv", ctx->in_path);
    } else {
        if (!ctx->dec_in_path) { set_result_ui(ctx, "Please choose a file to decrypt."); return; }
        ctx->in_path = g_strdup(ctx->dec_in_path);
        ctx->password = g_strdup(gtk_editable_get_text(GTK_EDITABLE(ctx->dec_pass_entry)));
        if (g_str_has_suffix(ctx->in_path, ".helv")) {
            ctx->out_path = g_strndup(ctx->in_path, strlen(ctx->in_path) - 5);
        } else if (g_str_has_suffix(ctx->in_path, ".enc")) {
            ctx->out_path = g_strndup(ctx->in_path, strlen(ctx->in_path) - 4);
        } else {
            ctx->out_path = g_strdup_printf("%s.decrypted", ctx->in_path);
        }
    }

    if (!*ctx->in_path || !*ctx->password) {
        set_result_ui(ctx, "Please select a file and enter a passkey.");
        g_free(ctx->in_path); g_free(ctx->out_path); g_free(ctx->password);
        return;
    }

    gtk_widget_set_sensitive(ctx->enc_btn, FALSE);
    gtk_widget_set_sensitive(ctx->dec_btn, FALSE);
    set_result_ui(ctx, "Processing... (Argon2id may take a moment)");
    
    crypto_worker(ctx);
    
    gtk_widget_set_sensitive(ctx->enc_btn, TRUE);
    gtk_widget_set_sensitive(ctx->dec_btn, TRUE);
    
    char *success_msg = g_strdup_printf("✓ %s successfully to %s", is_encrypt ? "Encrypted" : "Decrypted", ctx->out_path);
    set_result_ui(ctx, success_msg);
    g_free(success_msg);
    
    g_free(ctx->in_path); g_free(ctx->out_path); g_free(ctx->password);
}

static void on_encrypt(GtkButton *btn, gpointer ud) { (void)btn; start_crypto((CryptoCtx*)ud, TRUE); }
static void on_decrypt(GtkButton *btn, gpointer ud) { (void)btn; start_crypto((CryptoCtx*)ud, FALSE); }
static void on_reset_enc(GtkButton *btn, gpointer ud) {
    (void)btn;
    CryptoCtx *ctx = ud;
    g_free(ctx->enc_in_path); ctx->enc_in_path = NULL;
    gtk_label_set_text(GTK_LABEL(ctx->enc_file_lbl), "No file selected.");
    gtk_editable_set_text(GTK_EDITABLE(ctx->enc_pass_entry), "");
    gtk_label_set_text(GTK_LABEL(ctx->result_lbl), "");
    gtk_widget_set_visible(ctx->preview_box, FALSE);
    gtk_widget_set_visible(ctx->preview_empty, TRUE);
}
static void on_reset_dec(GtkButton *btn, gpointer ud) {
    (void)btn;
    CryptoCtx *ctx = ud;
    g_free(ctx->dec_in_path); ctx->dec_in_path = NULL;
    gtk_label_set_text(GTK_LABEL(ctx->dec_file_lbl), "No file selected.");
    gtk_editable_set_text(GTK_EDITABLE(ctx->dec_pass_entry), "");
    gtk_label_set_text(GTK_LABEL(ctx->result_lbl), "");
    gtk_widget_set_visible(ctx->preview_box, FALSE);
    gtk_widget_set_visible(ctx->preview_empty, TRUE);
}

static void update_preview(CryptoCtx *ctx, const char *path, gboolean is_encrypt) {
    char *basename = g_path_get_basename(path);
    GFile *f = g_file_new_for_path(path);
    goffset size = 0;
    GFileInfo *info = g_file_query_info(f, G_FILE_ATTRIBUTE_STANDARD_SIZE, G_FILE_QUERY_INFO_NONE, NULL, NULL);
    if (info) { size = g_file_info_get_size(info); g_object_unref(info); }
    g_object_unref(f);
    
    char *size_str = g_format_size(size);
    char *sz_txt = g_strdup_printf("Size: %s", size_str);
    
    gtk_widget_set_visible(ctx->preview_empty, FALSE);
    gtk_widget_set_visible(ctx->preview_box, TRUE);
    gtk_image_set_from_icon_name(GTK_IMAGE(ctx->preview_icon), is_encrypt ? "text-x-generic" : "emblem-readonly");
    gtk_label_set_text(GTK_LABEL(ctx->preview_name_lbl), basename);
    gtk_label_set_text(GTK_LABEL(ctx->preview_size_lbl), sz_txt);
    gtk_label_set_text(GTK_LABEL(ctx->preview_type_lbl), is_encrypt ? "Type: Raw File" : "Type: Encrypted Archive");
    gtk_label_set_text(GTK_LABEL(ctx->preview_status_lbl), is_encrypt ? "Ready to encrypt" : "Ready to decrypt");
    
    g_free(basename); g_free(size_str); g_free(sz_txt);
}

static void on_file_picked_enc(GObject *source, GAsyncResult *res, gpointer data) {
    CryptoCtx *ctx = data;
    GError *err = NULL;
    GFile *f = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source), res, &err);
    if (f) {
        g_free(ctx->enc_in_path);
        ctx->enc_in_path = g_file_get_path(f);
        char *base = g_path_get_basename(ctx->enc_in_path);
        gtk_label_set_text(GTK_LABEL(ctx->enc_file_lbl), base);
        g_free(base);
        update_preview(ctx, ctx->enc_in_path, TRUE);
        g_object_unref(f);
    }
}

static void on_file_picked_dec(GObject *source, GAsyncResult *res, gpointer data) {
    CryptoCtx *ctx = data;
    GError *err = NULL;
    GFile *f = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source), res, &err);
    if (f) {
        g_free(ctx->dec_in_path);
        ctx->dec_in_path = g_file_get_path(f);
        char *base = g_path_get_basename(ctx->dec_in_path);
        gtk_label_set_text(GTK_LABEL(ctx->dec_file_lbl), base);
        g_free(base);
        update_preview(ctx, ctx->dec_in_path, FALSE);
        g_object_unref(f);
    }
}

static void on_browse_enc(GtkButton *btn, gpointer ud) {
    (void)btn;
    GtkFileDialog *dlg = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dlg, "Choose file to encrypt");
    gtk_file_dialog_open(dlg, NULL, NULL, on_file_picked_enc, ud);
}

static void on_browse_dec(GtkButton *btn, gpointer ud) {
    (void)btn;
    GtkFileDialog *dlg = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dlg, "Upload encrypted file");
    gtk_file_dialog_open(dlg, NULL, NULL, on_file_picked_dec, ud);
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

static GtkWidget* create_enc_pane(CryptoCtx *ctx) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_size_request(box, 300, -1);
    
    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(header_box, GTK_ALIGN_CENTER);
    GtkWidget *icon = gtk_image_new_from_icon_name("changes-prevent-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 32);
    GtkWidget *title = gtk_label_new("Encrypt File");
    gtk_widget_add_css_class(title, "title-2");
    gtk_box_append(GTK_BOX(header_box), icon);
    gtk_box_append(GTK_BOX(header_box), title);
    
    GtkWidget *subtitle = gtk_label_new("Uploaded files are never stored, logged, or retained after encryption completes.");
    gtk_label_set_wrap(GTK_LABEL(subtitle), TRUE);
    gtk_widget_add_css_class(subtitle, "helvetia-fg-muted");
    gtk_widget_set_halign(subtitle, GTK_ALIGN_CENTER);
    
    gtk_box_append(GTK_BOX(box), header_box);
    gtk_box_append(GTK_BOX(box), subtitle);
    
    GtkWidget *group = adw_preferences_group_new();
    gtk_widget_set_margin_top(group, 8);
    
    GtkWidget *file_row = create_custom_file_row("Choose file to encrypt", &ctx->enc_file_lbl, G_CALLBACK(on_browse_enc), ctx);
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(group), file_row);
    
    GtkWidget *switch_row = adw_action_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(switch_row), "Use secret key for encryption");
    GtkWidget *sw = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(sw), TRUE);
    gtk_widget_set_valign(sw, GTK_ALIGN_CENTER);
    adw_action_row_add_suffix(ADW_ACTION_ROW(switch_row), sw);
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(group), switch_row);
    
    GtkWidget *pass_row = adw_password_entry_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(pass_row), "Enter encryption passkey");
    ctx->enc_pass_entry = pass_row;
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(group), pass_row);
    
    const char *algs[] = { "AES-256-GCM (Hardware)", "ChaCha20-Poly1305", NULL };
    ctx->enc_alg_combo = gtk_drop_down_new_from_strings(algs);
    gtk_widget_set_valign(ctx->enc_alg_combo, GTK_ALIGN_CENTER);
    GtkWidget *alg_row = adw_action_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(alg_row), "Algorithm");
    adw_action_row_add_suffix(ADW_ACTION_ROW(alg_row), ctx->enc_alg_combo);
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(group), alg_row);

    gtk_box_append(GTK_BOX(box), group);
    
    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    ctx->enc_btn = gtk_button_new_with_label("Encrypt File");
    gtk_widget_add_css_class(ctx->enc_btn, "suggested-action");
    GtkWidget *reset_btn = gtk_button_new_with_label("Reset");
    gtk_box_append(GTK_BOX(btn_box), ctx->enc_btn);
    gtk_box_append(GTK_BOX(btn_box), reset_btn);
    gtk_box_append(GTK_BOX(box), btn_box);
    
    g_signal_connect(ctx->enc_btn, "clicked", G_CALLBACK(on_encrypt), ctx);
    g_signal_connect(reset_btn, "clicked", G_CALLBACK(on_reset_enc), ctx);
    
    return box;
}

static GtkWidget* create_preview_pane(CryptoCtx *ctx) {
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
    GtkWidget *e_lbl = gtk_label_new("Drop file here\nor Browse");
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

static GtkWidget* create_dec_pane(CryptoCtx *ctx) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_size_request(box, 300, -1);
    
    // Header
    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(header_box, GTK_ALIGN_CENTER);
    GtkWidget *icon = gtk_image_new_from_icon_name("changes-allow-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 32);
    GtkWidget *title = gtk_label_new("Decrypt File");
    gtk_widget_add_css_class(title, "title-2");
    gtk_box_append(GTK_BOX(header_box), icon);
    gtk_box_append(GTK_BOX(header_box), title);
    
    GtkWidget *subtitle = gtk_label_new("Decrypt previously encrypted files securely using the correct secret key.");
    gtk_label_set_wrap(GTK_LABEL(subtitle), TRUE);
    gtk_widget_add_css_class(subtitle, "helvetia-fg-muted");
    gtk_widget_set_halign(subtitle, GTK_ALIGN_CENTER);
    
    gtk_box_append(GTK_BOX(box), header_box);
    gtk_box_append(GTK_BOX(box), subtitle);
    
    // Card
    GtkWidget *group = adw_preferences_group_new();
    gtk_widget_set_margin_top(group, 16);
    
    // File Row
    GtkWidget *file_row = create_custom_file_row("Upload encrypted file", &ctx->dec_file_lbl, G_CALLBACK(on_browse_dec), ctx);
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(group), file_row);
    
    GtkWidget *fmt_lbl = gtk_label_new("Ensure the secret key matches the one used during encryption.");
    gtk_widget_add_css_class(fmt_lbl, "helvetia-fg-muted");
    gtk_widget_set_halign(fmt_lbl, GTK_ALIGN_START);
    gtk_widget_set_margin_start(fmt_lbl, 12);
    gtk_widget_set_margin_bottom(fmt_lbl, 16);
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(group), fmt_lbl);
    
    // Switch Row
    GtkWidget *switch_row = adw_action_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(switch_row), "Secret key required");
    GtkWidget *sw = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(sw), TRUE);
    gtk_widget_set_valign(sw, GTK_ALIGN_CENTER);
    adw_action_row_add_suffix(ADW_ACTION_ROW(switch_row), sw);
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(group), switch_row);
    
    // Passkey
    GtkWidget *pass_row = adw_password_entry_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(pass_row), "Enter decryption passkey");
    ctx->dec_pass_entry = pass_row;
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(group), pass_row);
    
    gtk_box_append(GTK_BOX(box), group);
    
    // Buttons
    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_top(btn_box, 16);
    
    ctx->dec_btn = gtk_button_new_with_label("Decrypt File");
    gtk_widget_add_css_class(ctx->dec_btn, "suggested-action");
    GtkWidget *reset_btn = gtk_button_new_with_label("Reset");
    
    gtk_box_append(GTK_BOX(btn_box), ctx->dec_btn);
    gtk_box_append(GTK_BOX(btn_box), reset_btn);
    gtk_box_append(GTK_BOX(box), btn_box);
    
    g_signal_connect(ctx->dec_btn, "clicked", G_CALLBACK(on_decrypt), ctx);
    g_signal_connect(reset_btn, "clicked", G_CALLBACK(on_reset_dec), ctx);
    
    return box;
}

GtkWidget *build_file_encrypt(void) {
    CryptoCtx *ctx = g_new0(CryptoCtx, 1);
    
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(main_box, 24);
    gtk_widget_set_margin_end(main_box, 24);
    gtk_widget_set_margin_top(main_box, 24);
    gtk_widget_set_margin_bottom(main_box, 24);
    
    // Responsive FlowBox
    GtkWidget *flow = gtk_flow_box_new();
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flow), GTK_SELECTION_NONE);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flow), 3);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(flow), 1);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(flow), FALSE);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(flow), 32);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(flow), 32);
    gtk_widget_set_halign(flow, GTK_ALIGN_CENTER);
    
    GtkWidget *enc_pane = create_enc_pane(ctx);
    GtkWidget *preview_pane = create_preview_pane(ctx);
    GtkWidget *dec_pane = create_dec_pane(ctx);
    
    // Assign weights via hexpand and sizerequest
    gtk_widget_set_hexpand(enc_pane, TRUE);
    gtk_widget_set_hexpand(preview_pane, TRUE);
    gtk_widget_set_hexpand(dec_pane, TRUE);
    
    gtk_flow_box_append(GTK_FLOW_BOX(flow), enc_pane);
    gtk_flow_box_append(GTK_FLOW_BOX(flow), preview_pane);
    gtk_flow_box_append(GTK_FLOW_BOX(flow), dec_pane);
    
    gtk_box_append(GTK_BOX(main_box), flow);
    
    // Status Bar Area
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
