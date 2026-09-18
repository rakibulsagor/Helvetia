#define _POSIX_C_SOURCE 200809L
/* ================================================================
 * Helvetia — Security Module — Tool Implementations
 * ================================================================ */
#include "security_module.h"
#include "../../ui/widgets.h"
#include <gtk/gtk.h>
#include <glib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ================================================================
 * File Hash Calculator (MD5/SHA1/SHA256/SHA512 via sha*sum commands)
 * ================================================================ */
typedef struct { GtkWidget *entry, *result; } FileHashCtx;

static void on_file_hash(GtkButton *btn, gpointer ud) {
    (void)btn;
    FileHashCtx *ctx = ud;
    const char *algo = g_object_get_data(G_OBJECT(btn), "algo");
    const char *path = gtk_editable_get_text(GTK_EDITABLE(ctx->entry));
    if (!path || !*path) {
        gtk_label_set_text(GTK_LABEL(ctx->result), "⚠ Enter a file path");
        return;
    }
    char *qpath = g_shell_quote(path);
    char *cmd   = g_strdup_printf("%ssum %s 2>&1", algo, qpath);
    g_free(qpath);
    char *out = hv_run_cmd(cmd);
    g_free(cmd);
    gtk_label_set_text(GTK_LABEL(ctx->result), out ? out : "Error");
    g_free(out);
}

static void on_text_hash(GtkButton *btn, gpointer ud) {
    (void)btn;
    FileHashCtx *ctx = ud;
    const char *algo = g_object_get_data(G_OBJECT(btn), "algo");
    const char *text = gtk_editable_get_text(GTK_EDITABLE(ctx->entry));
    char *qtext = g_shell_quote(text ? text : "");
    char *cmd   = g_strdup_printf("printf %%s %s | %ssum 2>&1", qtext, algo);
    g_free(qtext);
    char *out = hv_run_cmd(cmd);
    g_free(cmd);
    /* Extract just the hash (first word) */
    if (out) {
        char *sp = strchr(out, ' ');
        if (sp) *sp = '\0';
        gtk_label_set_text(GTK_LABEL(ctx->result), out);
    }
    g_free(out);
}

static GtkWidget *make_hash_calculator(gboolean file_mode) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *entry;
    if (file_mode) {
        GtkWidget *row = hv_make_entry_row("File path:", &entry);
        gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "/path/to/file");
        gtk_box_append(GTK_BOX(box), row);
    } else {
        GtkWidget *row = hv_make_entry_row("Text:", &entry);
        gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Enter text to hash...");
        gtk_box_append(GTK_BOX(box), row);
    }

    GtkWidget *btns = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *result = hv_make_result_label();

    static const char *algos[] = { "md5", "sha1", "sha256", "sha512", NULL };
    static const char *labels[] = { "MD5", "SHA-1", "SHA-256", "SHA-512", NULL };

    FileHashCtx *ctx = g_new0(FileHashCtx, 1);
    ctx->entry = entry; ctx->result = result;
    GCallback cb = file_mode ? G_CALLBACK(on_file_hash) : G_CALLBACK(on_text_hash);

    for (int i = 0; algos[i]; i++) {
        GtkWidget *b = gtk_button_new_with_label(labels[i]);
        g_object_set_data(G_OBJECT(b), "algo", (gpointer)algos[i]);
        g_signal_connect(b, "clicked", cb, ctx);
        gtk_box_append(GTK_BOX(btns), b);
    }
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);

    gtk_box_append(GTK_BOX(box), btns);
    gtk_box_append(GTK_BOX(box), result);
    return box;
}

GtkWidget *build_file_hash_calculator(void) { return make_hash_calculator(TRUE);  }
GtkWidget *build_text_hash_calculator(void) { return make_hash_calculator(FALSE); }

/* ================================================================
 * Checksum File Creator / Verifier
 * ================================================================ */
typedef struct { GtkWidget *dir_entry, *result_tv; } ChecksumCtx;

static void on_checksum_create(GtkButton *btn, gpointer ud) {
    (void)btn;
    ChecksumCtx *ctx = ud;
    const char *dir = gtk_editable_get_text(GTK_EDITABLE(ctx->dir_entry));
    if (!dir || !*dir) {
        hv_textview_set_text(ctx->result_tv, "⚠ Enter a directory path");
        return;
    }
    char *qdir = g_shell_quote(dir);
    char *cmd  = g_strdup_printf("cd %s && find . -type f | sort | xargs sha256sum 2>&1", qdir);
    g_free(qdir);
    char *out = hv_run_cmd(cmd);
    g_free(cmd);
    hv_textview_set_text(ctx->result_tv, out ? out : "Error");
    g_free(out);
}

GtkWidget *build_checksum_creator(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *entry;
    gtk_box_append(GTK_BOX(box), hv_make_entry_row("Directory:", &entry));
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "/path/to/directory");

    GtkWidget *btn = hv_make_action_btn("Generate SHA-256 Checksums");
    GtkWidget *tv;
    GtkWidget *sw = hv_make_text_view(&tv, FALSE);

    ChecksumCtx *ctx = g_new0(ChecksumCtx, 1);
    ctx->dir_entry = entry; ctx->result_tv = tv;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_checksum_create), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);

    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), sw);
    return box;
}

/* ================================================================
 * Password Strength Checker
 * ================================================================ */
typedef struct { GtkWidget *entry, *strength_bar, *result; } PwStrengthCtx;

static void on_pw_check(GtkEditable *editable, gpointer ud) {
    PwStrengthCtx *ctx = ud;
    const char *pw = gtk_editable_get_text(editable);
    int score = 0;
    int len = (int)strlen(pw);

    if (len >= 8)  score++;
    if (len >= 12) score++;
    if (len >= 16) score++;

    gboolean has_upper = FALSE, has_lower = FALSE, has_digit = FALSE, has_sym = FALSE;
    for (const char *p = pw; *p; p++) {
        if (isupper((unsigned char)*p)) has_upper = TRUE;
        else if (islower((unsigned char)*p)) has_lower = TRUE;
        else if (isdigit((unsigned char)*p)) has_digit = TRUE;
        else has_sym = TRUE;
    }
    if (has_upper) score++;
    if (has_lower) score++;
    if (has_digit) score++;
    if (has_sym)   score += 2;

    double fraction = (double)score / 9.0;
    if (fraction > 1.0) fraction = 1.0;
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ctx->strength_bar), fraction);

    const char *label;
    if (fraction < 0.25)      label = "Very Weak";
    else if (fraction < 0.5)  label = "Weak";
    else if (fraction < 0.75) label = "Good";
    else if (fraction < 0.9)  label = "Strong";
    else                       label = "Very Strong";

    char msg[128];
    snprintf(msg, sizeof msg,
             "%s  |  Len: %d  |  Upper: %s  Lower: %s  Digits: %s  Symbols: %s",
             label, len,
             has_upper ? "✓" : "✗", has_lower ? "✓" : "✗",
             has_digit ? "✓" : "✗", has_sym ? "✓" : "✗");
    gtk_label_set_text(GTK_LABEL(ctx->result), msg);
}

GtkWidget *build_password_strength(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *entry;
    GtkWidget *row = hv_make_entry_row("Password:", &entry);
    gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
    gtk_box_append(GTK_BOX(box), row);

    GtkWidget *show_btn = gtk_check_button_new_with_label("Show password");
    g_signal_connect_swapped(show_btn, "toggled",
        G_CALLBACK(gtk_entry_set_visibility), entry);
    gtk_box_append(GTK_BOX(box), show_btn);

    GtkWidget *bar = gtk_progress_bar_new();
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(bar), FALSE);
    gtk_box_append(GTK_BOX(box), bar);

    GtkWidget *result = hv_make_result_label();
    gtk_label_set_text(GTK_LABEL(result), "Type a password above...");

    PwStrengthCtx *ctx = g_new0(PwStrengthCtx, 1);
    ctx->entry        = entry;
    ctx->strength_bar = bar;
    ctx->result       = result;
    g_signal_connect(entry, "changed", G_CALLBACK(on_pw_check), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);

    gtk_box_append(GTK_BOX(box), result);
    return box;
}

/* ================================================================
 * File Encrypt / Decrypt (via openssl CLI)
 * ================================================================ */
typedef struct { GtkWidget *in_entry, *out_entry, *pass_entry, *result; } CryptCtx;

static void do_crypt(GtkButton *btn, gpointer ud, gboolean encrypt) {
    (void)btn;
    CryptCtx *ctx = ud;
    const char *in_path  = gtk_editable_get_text(GTK_EDITABLE(ctx->in_entry));
    const char *out_path = gtk_editable_get_text(GTK_EDITABLE(ctx->out_entry));
    const char *pass     = gtk_editable_get_text(GTK_EDITABLE(ctx->pass_entry));

    if (!in_path || !*in_path || !out_path || !*out_path || !pass || !*pass) {
        gtk_label_set_text(GTK_LABEL(ctx->result), "⚠ Fill all fields");
        return;
    }
    char *qi = g_shell_quote(in_path);
    char *qo = g_shell_quote(out_path);
    char *qp = g_shell_quote(pass);
    char *cmd;
    if (encrypt) {
        cmd = g_strdup_printf(
            "openssl enc -aes-256-cbc -pbkdf2 -pass pass:%s -in %s -out %s 2>&1",
            qp, qi, qo);
    } else {
        cmd = g_strdup_printf(
            "openssl enc -d -aes-256-cbc -pbkdf2 -pass pass:%s -in %s -out %s 2>&1",
            qp, qi, qo);
    }
    g_free(qi); g_free(qo); g_free(qp);
    char *out = hv_run_cmd(cmd);
    g_free(cmd);
    if (!out || !*out)
        gtk_label_set_text(GTK_LABEL(ctx->result), encrypt ? "✓ File encrypted" : "✓ File decrypted");
    else
        gtk_label_set_text(GTK_LABEL(ctx->result), out);
    g_free(out);
}

static void on_encrypt(GtkButton *btn, gpointer ud) { do_crypt(btn, ud, TRUE);  }
static void on_decrypt(GtkButton *btn, gpointer ud) { do_crypt(btn, ud, FALSE); }

GtkWidget *build_file_encrypt(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *in_e, *out_e, *pass_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input file:", &in_e, FALSE));
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output file:", &out_e, TRUE));
    GtkWidget *pass_row = hv_make_entry_row("Passphrase:",  &pass_e);
    gtk_entry_set_visibility(GTK_ENTRY(pass_e), FALSE);
    gtk_box_append(GTK_BOX(box), pass_row);

    GtkWidget *info = gtk_label_new("Uses AES-256-CBC with PBKDF2 key derivation (OpenSSL)");
    gtk_widget_add_css_class(info, "helvetia-fg-muted");
    gtk_widget_set_halign(info, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box), info);

    GtkWidget *btns = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *enc  = hv_make_action_btn("🔒 Encrypt");
    GtkWidget *dec  = gtk_button_new_with_label("🔓 Decrypt");
    gtk_box_append(GTK_BOX(btns), enc);
    gtk_box_append(GTK_BOX(btns), dec);
    gtk_box_append(GTK_BOX(box), btns);

    GtkWidget *result = hv_make_result_label();

    CryptCtx *ctx = g_new0(CryptCtx, 1);
    ctx->in_entry   = in_e;
    ctx->out_entry  = out_e;
    ctx->pass_entry = pass_e;
    ctx->result     = result;
    g_signal_connect(enc, "clicked", G_CALLBACK(on_encrypt), ctx);
    g_signal_connect(dec, "clicked", G_CALLBACK(on_decrypt), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);

    gtk_box_append(GTK_BOX(box), result);
    return box;
}

/* ================================================================
 * Secure Delete
 * ================================================================ */
typedef struct { GtkWidget *entry, *passes_spin, *result; } ShredCtx;

static void on_shred(GtkButton *btn, gpointer ud) {
    (void)btn;
    ShredCtx *ctx = ud;
    const char *path = gtk_editable_get_text(GTK_EDITABLE(ctx->entry));
    int passes = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->passes_spin));
    if (!path || !*path) {
        gtk_label_set_text(GTK_LABEL(ctx->result), "⚠ Enter a file path");
        return;
    }
    char *qpath = g_shell_quote(path);
    char *cmd   = g_strdup_printf("shred -n %d -z -u %s 2>&1", passes, qpath);
    g_free(qpath);
    char *out = hv_run_cmd(cmd);
    g_free(cmd);
    if (!out || !*out)
        gtk_label_set_text(GTK_LABEL(ctx->result), "✓ File securely deleted");
    else
        gtk_label_set_text(GTK_LABEL(ctx->result), out);
    g_free(out);
}

GtkWidget *build_secure_delete(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *warn = gtk_label_new("⚠ WARNING: This permanently and irrecoverably destroys the file!");
    gtk_widget_add_css_class(warn, "helvetia-accent");
    gtk_widget_set_halign(warn, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box), warn);

    GtkWidget *entry;
    gtk_box_append(GTK_BOX(box), hv_make_entry_row("File to shred:", &entry));

    GtkWidget *pass_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(pass_row), gtk_label_new("Overwrite passes:"));
    GtkWidget *spin = gtk_spin_button_new_with_range(1, 35, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), 3);
    gtk_box_append(GTK_BOX(pass_row), spin);
    gtk_box_append(GTK_BOX(box), pass_row);

    GtkWidget *btn    = gtk_button_new_with_label("Securely Delete File");
    gtk_widget_add_css_class(btn, "destructive-action");
    GtkWidget *result = hv_make_result_label();

    ShredCtx *ctx = g_new0(ShredCtx, 1);
    ctx->entry       = entry;
    ctx->passes_spin = spin;
    ctx->result      = result;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_shred), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);

    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), result);
    return box;
}

/* ================================================================
 * OpenSSL X.509 Certificate Viewer
 * ================================================================ */
typedef struct { GtkWidget *entry, *tv; } CertCtx;

static void on_cert_view(GtkButton *btn, gpointer ud) {
    (void)btn;
    CertCtx *ctx = ud;
    const char *path = gtk_editable_get_text(GTK_EDITABLE(ctx->entry));
    if (!path || !*path) {
        hv_textview_set_text(ctx->tv, "⚠ Enter certificate path");
        return;
    }
    char *qpath = g_shell_quote(path);
    char *cmd   = g_strdup_printf("openssl x509 -in %s -text -noout 2>&1", qpath);
    g_free(qpath);
    char *out = hv_run_cmd(cmd);
    g_free(cmd);
    hv_textview_set_text(ctx->tv, out ? out : "Error reading certificate");
    g_free(out);
}

GtkWidget *build_cert_viewer(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *entry;
    gtk_box_append(GTK_BOX(box), hv_make_entry_row("Certificate (.pem/.crt):", &entry));
    GtkWidget *btn = hv_make_action_btn("View Certificate");
    GtkWidget *tv;
    GtkWidget *sw = hv_make_text_view(&tv, FALSE);

    CertCtx *ctx = g_new0(CertCtx, 1);
    ctx->entry = entry; ctx->tv = tv;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_cert_view), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);

    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), sw);
    return box;
}

/* ================================================================
 * SSH Key Generator
 * ================================================================ */
typedef struct { GtkWidget *type_dd, *bits_spin, *path_entry, *result; } SSHKeyCtx;

static void on_ssh_gen(GtkButton *btn, gpointer ud) {
    (void)btn;
    SSHKeyCtx *ctx = ud;
    static const char *types[] = { "rsa", "ed25519", "ecdsa" };
    guint idx = gtk_drop_down_get_selected(GTK_DROP_DOWN(ctx->type_dd));
    const char *type = types[idx < 3 ? idx : 0];
    int bits = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->bits_spin));
    const char *path = gtk_editable_get_text(GTK_EDITABLE(ctx->path_entry));
    if (!path || !*path) path = "/tmp/helvetia_key";

    char *qpath = g_shell_quote(path);
    char *cmd;
    if (strcmp(type, "rsa") == 0)
        cmd = g_strdup_printf("ssh-keygen -t rsa -b %d -N '' -f %s 2>&1", bits, qpath);
    else
        cmd = g_strdup_printf("ssh-keygen -t %s -N '' -f %s 2>&1", type, qpath);
    g_free(qpath);
    char *out = hv_run_cmd(cmd);
    g_free(cmd);
    gtk_label_set_text(GTK_LABEL(ctx->result), out ? out : "Error");
    g_free(out);
}

GtkWidget *build_ssh_keygen(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    GtkWidget *type_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(type_row), gtk_label_new("Key type:"));
    const char *types[] = { "RSA", "Ed25519", "ECDSA", NULL };
    GtkWidget *dd = gtk_drop_down_new_from_strings(types);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(dd), 1); /* ed25519 default */
    gtk_box_append(GTK_BOX(type_row), dd);
    gtk_box_append(GTK_BOX(box), type_row);

    GtkWidget *bits_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(bits_row), gtk_label_new("RSA bits:"));
    GtkWidget *spin = gtk_spin_button_new_with_range(1024, 8192, 1024);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), 4096);
    gtk_box_append(GTK_BOX(bits_row), spin);
    gtk_box_append(GTK_BOX(box), bits_row);

    GtkWidget *path_entry;
    GtkWidget *path_row = hv_make_file_picker_row("Output path:", &path_entry, TRUE);
    gtk_entry_set_placeholder_text(GTK_ENTRY(path_entry), "~/.ssh/id_ed25519");
    gtk_box_append(GTK_BOX(box), path_row);

    GtkWidget *btn    = hv_make_action_btn("Generate Key Pair");
    GtkWidget *result = hv_make_result_label();

    SSHKeyCtx *ctx = g_new0(SSHKeyCtx, 1);
    ctx->type_dd   = dd;
    ctx->bits_spin = spin;
    ctx->path_entry = path_entry;
    ctx->result    = result;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_ssh_gen), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);

    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), result);
    return box;
}
