/* ================================================================
 * Helvetia — Convert Module — Tool Implementations
 * ================================================================ */
#include "convert_module.h"
#include "../../ui/widgets.h"
#include <gtk/gtk.h>
#include <glib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ================================================================
 * Base64 Encode / Decode
 * ================================================================ */
typedef struct { GtkWidget *tv_in, *tv_out; } B64Ctx;

static void on_b64_encode(GtkButton *btn, gpointer ud) {
    (void)btn;
    B64Ctx *ctx = ud;
    char *input = hv_textview_get_text(ctx->tv_in);
    gchar *encoded = g_base64_encode((const guchar *)input, strlen(input));
    hv_textview_set_text(ctx->tv_out, encoded);
    g_free(encoded);
    g_free(input);
}

static void on_b64_decode(GtkButton *btn, gpointer ud) {
    (void)btn;
    B64Ctx *ctx = ud;
    char *input = hv_textview_get_text(ctx->tv_in);
    gsize out_len;
    guchar *decoded = g_base64_decode(input, &out_len);
    char *str = g_strndup((const char *)decoded, out_len);
    hv_textview_set_text(ctx->tv_out, str);
    g_free(str);
    g_free(decoded);
    g_free(input);
}

GtkWidget *build_base64_tool(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *tv_in, *tv_out;
    gtk_box_append(GTK_BOX(box), gtk_label_new("Input:"));
    GtkWidget *sw_in = hv_make_text_view(&tv_in, TRUE);
    gtk_widget_set_size_request(sw_in, -1, 130);
    gtk_box_append(GTK_BOX(box), sw_in);

    GtkWidget *btns = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *enc = hv_make_action_btn("Encode →");
    GtkWidget *dec = gtk_button_new_with_label("← Decode");
    gtk_box_append(GTK_BOX(btns), enc);
    gtk_box_append(GTK_BOX(btns), dec);
    gtk_box_append(GTK_BOX(box), btns);

    gtk_box_append(GTK_BOX(box), gtk_label_new("Output:"));
    GtkWidget *sw_out = hv_make_text_view(&tv_out, FALSE);
    gtk_widget_set_size_request(sw_out, -1, 130);
    gtk_box_append(GTK_BOX(box), sw_out);

    B64Ctx *ctx = g_new0(B64Ctx, 1);
    ctx->tv_in = tv_in; ctx->tv_out = tv_out;
    g_signal_connect(enc, "clicked", G_CALLBACK(on_b64_encode), ctx);
    g_signal_connect(dec, "clicked", G_CALLBACK(on_b64_decode), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);
    return box;
}

/* ================================================================
 * URL Encode / Decode
 * ================================================================ */
typedef struct { GtkWidget *tv_in, *tv_out; } UrlCtx;

static char *url_encode(const char *input) {
    GString *out = g_string_new(NULL);
    for (const char *p = input; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            g_string_append_c(out, (char)c);
        } else {
            g_string_append_printf(out, "%%%02X", c);
        }
    }
    return g_string_free(out, FALSE);
}

static char *url_decode(const char *input) {
    GString *out = g_string_new(NULL);
    for (const char *p = input; *p; p++) {
        if (*p == '%' && isxdigit((unsigned char)p[1]) && isxdigit((unsigned char)p[2])) {
            char hex[3] = { p[1], p[2], '\0' };
            g_string_append_c(out, (char)strtol(hex, NULL, 16));
            p += 2;
        } else if (*p == '+') {
            g_string_append_c(out, ' ');
        } else {
            g_string_append_c(out, *p);
        }
    }
    return g_string_free(out, FALSE);
}

static void on_url_encode(GtkButton *btn, gpointer ud) {
    (void)btn; UrlCtx *ctx = ud;
    char *input = hv_textview_get_text(ctx->tv_in);
    char *encoded = url_encode(input);
    hv_textview_set_text(ctx->tv_out, encoded);
    g_free(encoded); g_free(input);
}
static void on_url_decode(GtkButton *btn, gpointer ud) {
    (void)btn; UrlCtx *ctx = ud;
    char *input = hv_textview_get_text(ctx->tv_in);
    char *decoded = url_decode(input);
    hv_textview_set_text(ctx->tv_out, decoded);
    g_free(decoded); g_free(input);
}

GtkWidget *build_url_encoder(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *tv_in, *tv_out;
    gtk_box_append(GTK_BOX(box), gtk_label_new("Input:"));
    GtkWidget *sw_in = hv_make_text_view(&tv_in, TRUE);
    gtk_widget_set_size_request(sw_in, -1, 100);
    gtk_box_append(GTK_BOX(box), sw_in);

    GtkWidget *btns = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *enc  = hv_make_action_btn("URL Encode →");
    GtkWidget *dec  = gtk_button_new_with_label("← URL Decode");
    gtk_box_append(GTK_BOX(btns), enc);
    gtk_box_append(GTK_BOX(btns), dec);
    gtk_box_append(GTK_BOX(box), btns);

    gtk_box_append(GTK_BOX(box), gtk_label_new("Output:"));
    GtkWidget *sw_out = hv_make_text_view(&tv_out, FALSE);
    gtk_widget_set_size_request(sw_out, -1, 100);
    gtk_box_append(GTK_BOX(box), sw_out);

    UrlCtx *ctx = g_new0(UrlCtx, 1);
    ctx->tv_in = tv_in; ctx->tv_out = tv_out;
    g_signal_connect(enc, "clicked", G_CALLBACK(on_url_encode), ctx);
    g_signal_connect(dec, "clicked", G_CALLBACK(on_url_decode), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);
    return box;
}

/* ================================================================
 * Hex Encode / Decode
 * ================================================================ */
typedef struct { GtkWidget *tv_in, *tv_out; } HexCtx;

static void on_hex_encode(GtkButton *btn, gpointer ud) {
    (void)btn; HexCtx *ctx = ud;
    char *input = hv_textview_get_text(ctx->tv_in);
    GString *out = g_string_new(NULL);
    for (const char *p = input; *p; p++)
        g_string_append_printf(out, "%02X ", (unsigned char)*p);
    hv_textview_set_text(ctx->tv_out, out->str);
    g_string_free(out, TRUE); g_free(input);
}
static void on_hex_decode(GtkButton *btn, gpointer ud) {
    (void)btn; HexCtx *ctx = ud;
    char *input = hv_textview_get_text(ctx->tv_in);
    GString *out = g_string_new(NULL);
    char *p = input;
    while (*p) {
        while (isspace((unsigned char)*p)) p++;
        if (!*p) break;
        if (isxdigit((unsigned char)p[0]) && isxdigit((unsigned char)p[1])) {
            char hex[3] = { p[0], p[1], '\0' };
            g_string_append_c(out, (char)strtol(hex, NULL, 16));
            p += 2;
        } else { p++; }
    }
    hv_textview_set_text(ctx->tv_out, out->str);
    g_string_free(out, TRUE); g_free(input);
}

GtkWidget *build_hex_encoder(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *tv_in, *tv_out;
    gtk_box_append(GTK_BOX(box), gtk_label_new("Input:"));
    GtkWidget *sw_in = hv_make_text_view(&tv_in, TRUE);
    gtk_widget_set_size_request(sw_in, -1, 100);
    gtk_box_append(GTK_BOX(box), sw_in);

    GtkWidget *btns = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *enc  = hv_make_action_btn("Text → Hex");
    GtkWidget *dec  = gtk_button_new_with_label("Hex → Text");
    gtk_box_append(GTK_BOX(btns), enc);
    gtk_box_append(GTK_BOX(btns), dec);
    gtk_box_append(GTK_BOX(box), btns);

    gtk_box_append(GTK_BOX(box), gtk_label_new("Output:"));
    GtkWidget *sw_out = hv_make_text_view(&tv_out, FALSE);
    gtk_widget_set_size_request(sw_out, -1, 100);
    gtk_box_append(GTK_BOX(box), sw_out);

    HexCtx *ctx = g_new0(HexCtx, 1);
    ctx->tv_in = tv_in; ctx->tv_out = tv_out;
    g_signal_connect(enc, "clicked", G_CALLBACK(on_hex_encode), ctx);
    g_signal_connect(dec, "clicked", G_CALLBACK(on_hex_decode), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);
    return box;
}

/* ================================================================
 * ROT13
 * ================================================================ */
typedef struct { GtkWidget *tv_in, *tv_out; } Rot13Ctx;

static void on_rot13(GtkButton *btn, gpointer ud) {
    (void)btn; Rot13Ctx *ctx = ud;
    char *input = hv_textview_get_text(ctx->tv_in);
    char *out = g_strdup(input);
    for (char *p = out; *p; p++) {
        if (*p >= 'a' && *p <= 'z') *p = (char)((*p - 'a' + 13) % 26 + 'a');
        else if (*p >= 'A' && *p <= 'Z') *p = (char)((*p - 'A' + 13) % 26 + 'A');
    }
    hv_textview_set_text(ctx->tv_out, out);
    g_free(out); g_free(input);
}

GtkWidget *build_rot13(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *tv_in, *tv_out;
    GtkWidget *note = gtk_label_new("ROT13 is its own inverse — apply twice to decode");
    gtk_widget_add_css_class(note, "helvetia-fg-muted");
    gtk_widget_set_halign(note, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box), note);

    GtkWidget *sw_in = hv_make_text_view(&tv_in, TRUE);
    gtk_widget_set_size_request(sw_in, -1, 140);
    gtk_box_append(GTK_BOX(box), sw_in);

    GtkWidget *btn = hv_make_action_btn("Apply ROT13");
    gtk_box_append(GTK_BOX(box), btn);

    GtkWidget *sw_out = hv_make_text_view(&tv_out, FALSE);
    gtk_widget_set_size_request(sw_out, -1, 140);
    gtk_box_append(GTK_BOX(box), sw_out);

    Rot13Ctx *ctx = g_new0(Rot13Ctx, 1);
    ctx->tv_in = tv_in; ctx->tv_out = tv_out;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_rot13), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);
    return box;
}

/* ================================================================
 * JSON Formatter
 * ================================================================ */
typedef struct { GtkWidget *tv_in, *tv_out; } JsonCtx;

static void on_json_format(GtkButton *btn, gpointer ud) {
    (void)btn;
    JsonCtx *ctx = ud;
    char *input = hv_textview_get_text(ctx->tv_in);
    char *cmd = g_strdup_printf("echo %s | python3 -m json.tool 2>&1",
                                 g_shell_quote(input));
    g_free(input);
    char *output = hv_run_cmd(cmd);
    g_free(cmd);
    hv_textview_set_text(ctx->tv_out, output ? output : "⚠ Error formatting JSON");
    g_free(output);
}

static void on_json_minify(GtkButton *btn, gpointer ud) {
    (void)btn;
    JsonCtx *ctx = ud;
    char *input = hv_textview_get_text(ctx->tv_in);
    char *cmd = g_strdup_printf(
        "echo %s | python3 -c \"import sys,json; print(json.dumps(json.load(sys.stdin)))\" 2>&1",
        g_shell_quote(input));
    g_free(input);
    char *output = hv_run_cmd(cmd);
    g_free(cmd);
    hv_textview_set_text(ctx->tv_out, output ? output : "⚠ Invalid JSON");
    g_free(output);
}

GtkWidget *build_json_formatter(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *tv_in, *tv_out;
    gtk_box_append(GTK_BOX(box), gtk_label_new("JSON Input:"));
    GtkWidget *sw_in = hv_make_text_view(&tv_in, TRUE);
    gtk_widget_set_size_request(sw_in, -1, 180);
    gtk_box_append(GTK_BOX(box), sw_in);

    GtkWidget *btns  = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *fmt   = hv_make_action_btn("Format / Pretty-print");
    GtkWidget *mini  = gtk_button_new_with_label("Minify");
    gtk_box_append(GTK_BOX(btns), fmt);
    gtk_box_append(GTK_BOX(btns), mini);
    gtk_box_append(GTK_BOX(box), btns);

    gtk_box_append(GTK_BOX(box), gtk_label_new("Output:"));
    GtkWidget *sw_out = hv_make_text_view(&tv_out, FALSE);
    gtk_widget_set_size_request(sw_out, -1, 180);
    gtk_box_append(GTK_BOX(box), sw_out);

    JsonCtx *ctx = g_new0(JsonCtx, 1);
    ctx->tv_in = tv_in; ctx->tv_out = tv_out;
    g_signal_connect(fmt,  "clicked", G_CALLBACK(on_json_format), ctx);
    g_signal_connect(mini, "clicked", G_CALLBACK(on_json_minify), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);
    return box;
}

/* ================================================================
 * Line Ending Converter
 * ================================================================ */
typedef struct { GtkWidget *tv_in, *tv_out, *result; } LineEndCtx;

static void on_line_convert(GtkButton *btn, gpointer ud) {
    (void)btn;
    LineEndCtx *ctx = ud;
    const char *to = g_object_get_data(G_OBJECT(btn), "target-le");
    char *input = hv_textview_get_text(ctx->tv_in);
    GString *out = g_string_new(NULL);
    int changed = 0;
    for (const char *p = input; *p; p++) {
        if (*p == '\r' && *(p+1) == '\n') {
            /* CRLF */
            if (strcmp(to, "lf") == 0)   { g_string_append_c(out, '\n'); changed++; p++; }
            else if (strcmp(to, "cr") == 0) { g_string_append_c(out, '\r'); changed++; p++; }
            else { g_string_append(out, "\r\n"); p++; }
        } else if (*p == '\r') {
            if (strcmp(to, "lf") == 0)   { g_string_append_c(out, '\n'); changed++; }
            else if (strcmp(to, "crlf") == 0) { g_string_append(out, "\r\n"); changed++; }
            else { g_string_append_c(out, '\r'); }
        } else if (*p == '\n') {
            if (strcmp(to, "cr") == 0)   { g_string_append_c(out, '\r'); changed++; }
            else if (strcmp(to, "crlf") == 0) { g_string_append(out, "\r\n"); changed++; }
            else { g_string_append_c(out, '\n'); }
        } else {
            g_string_append_c(out, *p);
        }
    }
    hv_textview_set_text(ctx->tv_out, out->str);
    char stat[64];
    snprintf(stat, sizeof stat, "%d line endings converted", changed);
    gtk_label_set_text(GTK_LABEL(ctx->result), stat);
    g_string_free(out, TRUE);
    g_free(input);
}

GtkWidget *build_line_ending_converter(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *tv_in, *tv_out;
    gtk_box_append(GTK_BOX(box), gtk_label_new("Input text:"));
    GtkWidget *sw_in = hv_make_text_view(&tv_in, TRUE);
    gtk_widget_set_size_request(sw_in, -1, 140);
    gtk_box_append(GTK_BOX(box), sw_in);

    GtkWidget *btns  = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *to_lf   = hv_make_action_btn("→ LF (Unix)");
    GtkWidget *to_crlf = gtk_button_new_with_label("→ CRLF (Windows)");
    GtkWidget *to_cr   = gtk_button_new_with_label("→ CR (Old Mac)");
    g_object_set_data(G_OBJECT(to_lf),   "target-le", "lf");
    g_object_set_data(G_OBJECT(to_crlf), "target-le", "crlf");
    g_object_set_data(G_OBJECT(to_cr),   "target-le", "cr");
    gtk_box_append(GTK_BOX(btns), to_lf);
    gtk_box_append(GTK_BOX(btns), to_crlf);
    gtk_box_append(GTK_BOX(btns), to_cr);
    gtk_box_append(GTK_BOX(box), btns);

    GtkWidget *result = hv_make_result_label();
    gtk_box_append(GTK_BOX(box), result);

    gtk_box_append(GTK_BOX(box), gtk_label_new("Output:"));
    GtkWidget *sw_out = hv_make_text_view(&tv_out, FALSE);
    gtk_widget_set_size_request(sw_out, -1, 140);
    gtk_box_append(GTK_BOX(box), sw_out);

    LineEndCtx *ctx = g_new0(LineEndCtx, 1);
    ctx->tv_in = tv_in; ctx->tv_out = tv_out; ctx->result = result;
    g_signal_connect(to_lf,   "clicked", G_CALLBACK(on_line_convert), ctx);
    g_signal_connect(to_crlf, "clicked", G_CALLBACK(on_line_convert), ctx);
    g_signal_connect(to_cr,   "clicked", G_CALLBACK(on_line_convert), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);
    return box;
}

/* ================================================================
 * File Type Detector
 * ================================================================ */
typedef struct { GtkWidget *entry, *result; } FileCtx;

static void on_file_detect(GtkButton *btn, gpointer ud) {
    (void)btn;
    FileCtx *ctx = ud;
    const char *path = gtk_editable_get_text(GTK_EDITABLE(ctx->entry));
    if (!path || !*path) {
        gtk_label_set_text(GTK_LABEL(ctx->result), "⚠ Enter a file path");
        return;
    }
    char *quoted = g_shell_quote(path);
    char *cmd = g_strdup_printf("file %s 2>&1", quoted);
    g_free(quoted);
    char *out = hv_run_cmd(cmd);
    g_free(cmd);
    gtk_label_set_text(GTK_LABEL(ctx->result), out ? out : "Error running 'file'");
    g_free(out);
}

GtkWidget *build_file_type_detector(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *note = gtk_label_new("Detects file type using the system magic database");
    gtk_widget_add_css_class(note, "helvetia-fg-muted");
    gtk_widget_set_halign(note, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box), note);

    GtkWidget *entry;
    GtkWidget *row = hv_make_file_picker_row("File path:", &entry, FALSE);
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "/path/to/file");
    gtk_box_append(GTK_BOX(box), row);

    GtkWidget *btn    = hv_make_action_btn("Detect Type");
    GtkWidget *result = hv_make_result_label();

    FileCtx *ctx = g_new0(FileCtx, 1);
    ctx->entry = entry; ctx->result = result;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_file_detect), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);

    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), result);
    return box;
}
