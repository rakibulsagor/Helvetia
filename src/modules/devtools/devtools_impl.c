#define _POSIX_C_SOURCE 200809L
/* ================================================================
 * Helvetia — Dev Tools Module — Tool Implementations
 * ================================================================ */
#include "devtools_module.h"
#include "../../ui/widgets.h"
#include <gtk/gtk.h>
#include <glib.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

/* ================================================================
 * UUID Generator
 * ================================================================ */
typedef struct { GtkWidget *tv; } UUIDCtx;

static void on_uuid_gen(GtkButton *btn, gpointer ud) {
    (void)btn;
    UUIDCtx *ctx = ud;
    char *out = hv_run_cmd("cat /proc/sys/kernel/random/uuid 2>/dev/null || "
                           "python3 -c 'import uuid; print(uuid.uuid4())' 2>&1");
    if (out) {
        /* Strip newline */
        char *nl = strchr(out, '\n');
        if (nl) *nl = '\0';
        hv_textview_set_text(ctx->tv, out);
        GdkClipboard *cb = gdk_display_get_clipboard(gdk_display_get_default());
        gdk_clipboard_set_text(cb, out);
    }
    g_free(out);
}

static void on_uuid_gen_bulk(GtkButton *btn, gpointer ud) {
    (void)btn;
    UUIDCtx *ctx = ud;
    GString *result = g_string_new(NULL);
    for (int i = 0; i < 10; i++) {
        char *uuid = hv_run_cmd("cat /proc/sys/kernel/random/uuid 2>/dev/null");
        if (uuid) {
            char *nl = strchr(uuid, '\n');
            if (nl) *nl = '\0';
            g_string_append(result, uuid);
            g_string_append_c(result, '\n');
            g_free(uuid);
        }
    }
    hv_textview_set_text(ctx->tv, result->str);
    g_string_free(result, TRUE);
}

GtkWidget *build_uuid_generator(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *tv;
    GtkWidget *sw = hv_make_text_view(&tv, FALSE);
    gtk_widget_set_size_request(sw, -1, 200);

    UUIDCtx *ctx = g_new0(UUIDCtx, 1);
    ctx->tv = tv;

    GtkWidget *btns = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *btn1 = hv_make_action_btn("Generate UUID v4");
    GtkWidget *btn2 = gtk_button_new_with_label("Generate 10 UUIDs");
    gtk_box_append(GTK_BOX(btns), btn1);
    gtk_box_append(GTK_BOX(btns), btn2);

    GtkWidget *note = gtk_label_new("Single UUID is automatically copied to clipboard");
    gtk_widget_add_css_class(note, "helvetia-fg-muted");
    gtk_widget_set_halign(note, GTK_ALIGN_START);

    g_signal_connect(btn1, "clicked", G_CALLBACK(on_uuid_gen),      ctx);
    g_signal_connect(btn2, "clicked", G_CALLBACK(on_uuid_gen_bulk), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);

    gtk_box_append(GTK_BOX(box), btns);
    gtk_box_append(GTK_BOX(box), note);
    gtk_box_append(GTK_BOX(box), sw);
    return box;
}

/* ================================================================
 * JWT Decoder
 * ================================================================ */
typedef struct { GtkWidget *tv_in, *tv_header, *tv_payload; } JWTCtx;

static void on_jwt_decode(GtkButton *btn, gpointer ud) {
    (void)btn;
    JWTCtx *ctx = ud;
    char *token = hv_textview_get_text(ctx->tv_in);
    if (!token || !*token) { g_free(token); return; }
    
    /* JWT is header.payload.signature, each base64url encoded */
    char *dot1 = strchr(token, '.');
    char *dot2 = dot1 ? strchr(dot1 + 1, '.') : NULL;
    
    if (!dot1 || !dot2) {
        hv_textview_set_text(ctx->tv_header, "⚠ Invalid JWT format (expected 3 parts)");
        hv_textview_set_text(ctx->tv_payload, "");
        g_free(token);
        return;
    }
    
    /* Decode header */
    char *header_b64 = g_strndup(token, dot1 - token);
    char *payload_b64 = g_strndup(dot1 + 1, dot2 - dot1 - 1);
    
    /* Convert base64url to base64 */
    /* (b64url decode helper not needed, using python subprocess) */
    
    /* Use python to pretty-print the JSON */
    char *cmd_h = g_strdup_printf(
        "python3 -c \"import base64,json,sys; "
        "s='%s'; pad=s+'=='[:(-len(s))%%4]; "
        "print(json.dumps(json.loads(base64.urlsafe_b64decode(pad).decode()),indent=2))\" 2>&1",
        header_b64);
    char *cmd_p = g_strdup_printf(
        "python3 -c \"import base64,json,sys; "
        "s='%s'; pad=s+'=='[:(-len(s))%%4]; "
        "print(json.dumps(json.loads(base64.urlsafe_b64decode(pad).decode()),indent=2))\" 2>&1",
        payload_b64);
    
    char *header_json  = hv_run_cmd(cmd_h);
    char *payload_json = hv_run_cmd(cmd_p);
    
    hv_textview_set_text(ctx->tv_header, header_json  ? header_json  : header_b64);
    hv_textview_set_text(ctx->tv_payload, payload_json ? payload_json : payload_b64);
    
    g_free(cmd_h); g_free(cmd_p);
    g_free(header_json); g_free(payload_json);
    g_free(header_b64); g_free(payload_b64);
    g_free(token);
}

GtkWidget *build_jwt_decoder(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *tv_in;
    gtk_box_append(GTK_BOX(box), gtk_label_new("JWT Token:"));
    GtkWidget *sw_in = hv_make_text_view(&tv_in, TRUE);
    gtk_widget_set_size_request(sw_in, -1, 80);
    gtk_box_append(GTK_BOX(box), sw_in);

    GtkWidget *btn = hv_make_action_btn("Decode JWT");
    gtk_box_append(GTK_BOX(box), btn);

    GtkWidget *header_lbl = gtk_label_new("Header:");
    gtk_widget_set_halign(header_lbl, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box), header_lbl);
    GtkWidget *tv_header;
    GtkWidget *sw_h = hv_make_text_view(&tv_header, FALSE);
    gtk_widget_set_size_request(sw_h, -1, 100);
    gtk_box_append(GTK_BOX(box), sw_h);

    GtkWidget *payload_lbl = gtk_label_new("Payload:");
    gtk_widget_set_halign(payload_lbl, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box), payload_lbl);
    GtkWidget *tv_payload;
    GtkWidget *sw_p = hv_make_text_view(&tv_payload, FALSE);
    gtk_widget_set_size_request(sw_p, -1, 140);
    gtk_box_append(GTK_BOX(box), sw_p);

    JWTCtx *ctx = g_new0(JWTCtx, 1);
    ctx->tv_in = tv_in; ctx->tv_header = tv_header; ctx->tv_payload = tv_payload;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_jwt_decode), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);
    return box;
}

/* ================================================================
 * Unix Timestamp Converter (Dev Tools variant)
 * ================================================================ */
GtkWidget *build_dev_timestamp(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    /* Now button */
    GtkWidget *now_btn = gtk_button_new_with_label("📋 Current Time");
    gtk_widget_add_css_class(now_btn, "flat");

    GtkWidget *ts_entry;
    GtkWidget *row = hv_make_entry_row("Unix Timestamp:", &ts_entry);
    gtk_box_append(GTK_BOX(box), row);

    GtkWidget *human_entry;
    GtkWidget *row2 = hv_make_entry_row("Human date (UTC):", &human_entry);
    gtk_box_append(GTK_BOX(box), row2);

    GtkWidget *result = hv_make_result_label();

    /* Pre-fill with now */
    char now_str[32];
    snprintf(now_str, sizeof now_str, "%ld", (long)time(NULL));
    gtk_editable_set_text(GTK_EDITABLE(ts_entry), now_str);

    /* Wire now button */
    g_signal_connect_swapped(now_btn, "clicked",
        G_CALLBACK(gtk_editable_set_text),
        ts_entry);
    /* Just connect a lambda-free approach: when now_btn clicked, update entry with current time */

    gtk_box_append(GTK_BOX(box), result);
    return box;
}

/* ================================================================
 * Number Base Converter (Dev Tools variant)
 * ================================================================ */
typedef struct { GtkWidget *entry, *dec, *hex, *bin, *oct; } BaseCtx2;

static void on_base_input(GtkEditable *e, gpointer ud) {
    BaseCtx2 *ctx = ud;
    const char *text = gtk_editable_get_text(e);
    if (!text || !*text) return;
    
    long long val = 0;
    if (e == GTK_EDITABLE(ctx->entry)) {
        val = strtoll(text, NULL, 0); /* auto-detect 0x/0b/0 prefix */
    } else if (e == GTK_EDITABLE(ctx->dec)) {
        val = strtoll(text, NULL, 10);
    } else if (e == GTK_EDITABLE(ctx->hex)) {
        val = strtoll(text, NULL, 16);
    } else if (e == GTK_EDITABLE(ctx->oct)) {
        val = strtoll(text, NULL, 8);
    }

    char dec_s[32], hex_s[32], oct_s[32], bin_s[68];
    snprintf(dec_s, sizeof dec_s, "%lld", val);
    snprintf(hex_s, sizeof hex_s, "0x%llX", val);
    snprintf(oct_s, sizeof oct_s, "0%llo", val);
    
    /* Binary */
    if (val == 0) { strcpy(bin_s, "0b0"); }
    else {
        char tmp[66] = {0};
        int pos = 64;
        long long v = val < 0 ? -val : val;
        while (v > 0) { tmp[pos--] = '0' + (v & 1); v >>= 1; }
        snprintf(bin_s, sizeof bin_s, "0b%s", tmp + pos + 1);
    }

    /* Temporarily block signals to avoid recursive updates */
    g_signal_handlers_block_by_func(ctx->dec, on_base_input, ctx);
    g_signal_handlers_block_by_func(ctx->hex, on_base_input, ctx);
    g_signal_handlers_block_by_func(ctx->oct, on_base_input, ctx);
    
    if (e != GTK_EDITABLE(ctx->dec)) gtk_editable_set_text(GTK_EDITABLE(ctx->dec), dec_s);
    if (e != GTK_EDITABLE(ctx->hex)) gtk_editable_set_text(GTK_EDITABLE(ctx->hex), hex_s);
    if (e != GTK_EDITABLE(ctx->oct)) gtk_editable_set_text(GTK_EDITABLE(ctx->oct), oct_s);
    if (ctx->bin) gtk_label_set_text(GTK_LABEL(ctx->bin), bin_s);
    
    g_signal_handlers_unblock_by_func(ctx->dec, on_base_input, ctx);
    g_signal_handlers_unblock_by_func(ctx->hex, on_base_input, ctx);
    g_signal_handlers_unblock_by_func(ctx->oct, on_base_input, ctx);
}

GtkWidget *build_number_base_dev(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    GtkWidget *dec_e, *hex_e, *oct_e;
    GtkWidget *r1 = hv_make_entry_row("Decimal:",     &dec_e);
    GtkWidget *r2 = hv_make_entry_row("Hexadecimal:", &hex_e);
    GtkWidget *r3 = hv_make_entry_row("Octal:",       &oct_e);
    gtk_entry_set_placeholder_text(GTK_ENTRY(dec_e), "e.g. 255");
    gtk_entry_set_placeholder_text(GTK_ENTRY(hex_e), "e.g. 0xFF");
    gtk_entry_set_placeholder_text(GTK_ENTRY(oct_e), "e.g. 0377");

    GtkWidget *bin_lbl_head = gtk_label_new("Binary:");
    gtk_widget_set_halign(bin_lbl_head, GTK_ALIGN_START);
    GtkWidget *bin_result = hv_make_result_label();

    BaseCtx2 *ctx = g_new0(BaseCtx2, 1);
    ctx->entry = dec_e; /* unused */
    ctx->dec   = dec_e;
    ctx->hex   = hex_e;
    ctx->oct   = oct_e;
    ctx->bin   = bin_result;

    g_signal_connect(dec_e, "changed", G_CALLBACK(on_base_input), ctx);
    g_signal_connect(hex_e, "changed", G_CALLBACK(on_base_input), ctx);
    g_signal_connect(oct_e, "changed", G_CALLBACK(on_base_input), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);

    gtk_box_append(GTK_BOX(box), r1);
    gtk_box_append(GTK_BOX(box), r2);
    gtk_box_append(GTK_BOX(box), r3);
    gtk_box_append(GTK_BOX(box), bin_lbl_head);
    gtk_box_append(GTK_BOX(box), bin_result);
    return box;
}

/* ================================================================
 * HTTP Status Code Lookup
 * ================================================================ */
typedef struct { const char *code; const char *message; const char *desc; } HttpStatus;
static const HttpStatus http_statuses[] = {
    {"100", "Continue", "Server received request headers, client should proceed"},
    {"200", "OK", "Standard response for successful HTTP requests"},
    {"201", "Created", "Request fulfilled, new resource created"},
    {"204", "No Content", "Request succeeded, no response body"},
    {"301", "Moved Permanently", "Resource permanently moved to new URL"},
    {"302", "Found", "Resource temporarily at different URL"},
    {"304", "Not Modified", "Resource not modified since last request"},
    {"400", "Bad Request", "Server could not understand the request"},
    {"401", "Unauthorized", "Authentication required"},
    {"403", "Forbidden", "Server understood but refuses to authorize"},
    {"404", "Not Found", "Requested resource not found"},
    {"405", "Method Not Allowed", "HTTP method not supported"},
    {"408", "Request Timeout", "Server timed out waiting for request"},
    {"409", "Conflict", "Request conflicts with server state"},
    {"410", "Gone", "Resource permanently removed"},
    {"422", "Unprocessable Entity", "Request well-formed but semantic errors"},
    {"429", "Too Many Requests", "Client sent too many requests (rate limiting)"},
    {"500", "Internal Server Error", "Generic server-side error"},
    {"502", "Bad Gateway", "Server received invalid response from upstream"},
    {"503", "Service Unavailable", "Server temporarily unavailable"},
    {"504", "Gateway Timeout", "Upstream server didn't respond in time"},
    {NULL, NULL, NULL}
};

typedef struct { GtkWidget *entry, *result; } HTTPCtx;

static void on_http_lookup(GtkEditable *e, gpointer ud) {
    HTTPCtx *ctx = ud;
    const char *query = gtk_editable_get_text(e);
    if (!query || strlen(query) < 3) { gtk_label_set_text(GTK_LABEL(ctx->result), "—"); return; }
    
    for (int i = 0; http_statuses[i].code; i++) {
        if (strncmp(http_statuses[i].code, query, 3) == 0) {
            char buf[256];
            snprintf(buf, sizeof buf, "%s %s\n%s",
                     http_statuses[i].code, http_statuses[i].message,
                     http_statuses[i].desc);
            gtk_label_set_text(GTK_LABEL(ctx->result), buf);
            return;
        }
    }
    gtk_label_set_text(GTK_LABEL(ctx->result), "Unknown status code");
}

GtkWidget *build_http_status_lookup(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *entry;
    GtkWidget *row = hv_make_entry_row("Status code:", &entry);
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "e.g. 404");
    gtk_entry_set_max_length(GTK_ENTRY(entry), 3);
    gtk_box_append(GTK_BOX(box), row);

    GtkWidget *result = hv_make_result_label();
    gtk_label_set_wrap(GTK_LABEL(result), TRUE);

    HTTPCtx *ctx = g_new0(HTTPCtx, 1);
    ctx->entry = entry; ctx->result = result;
    g_signal_connect(entry, "changed", G_CALLBACK(on_http_lookup), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);

    /* Quick access buttons */
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 4);
    const char *common[] = {"200","201","204","301","302","400","401","403","404","500","502","503",NULL};
    for (int i = 0; common[i]; i++) {
        GtkWidget *b = gtk_button_new_with_label(common[i]);
        gtk_widget_add_css_class(b, "flat");
        g_object_set_data(G_OBJECT(b), "code", (gpointer)common[i]);
        g_signal_connect_swapped(b, "clicked",
            G_CALLBACK(gtk_editable_set_text), entry);
        gtk_grid_attach(GTK_GRID(grid), b, i % 6, i / 6, 1, 1);
    }
    gtk_box_append(GTK_BOX(box), grid);
    gtk_box_append(GTK_BOX(box), result);
    return box;
}

/* ================================================================
 * Hex Viewer
 * ================================================================ */
typedef struct { GtkWidget *entry, *tv; } HexViewCtx;

static void on_hex_view(GtkButton *btn, gpointer ud) {
    (void)btn;
    HexViewCtx *ctx = ud;
    const char *path = gtk_editable_get_text(GTK_EDITABLE(ctx->entry));
    if (!path || !*path) {
        hv_textview_set_text(ctx->tv, "⚠ Enter a file path");
        return;
    }
    char *qpath = g_shell_quote(path);
    char *cmd   = g_strdup_printf("xxd %s 2>&1 | head -100", qpath);
    g_free(qpath);
    char *out = hv_run_cmd(cmd);
    g_free(cmd);
    hv_textview_set_text(ctx->tv, out ? out : "Error");
    g_free(out);
}

GtkWidget *build_hex_viewer(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *entry;
    gtk_box_append(GTK_BOX(box), hv_make_entry_row("File path:", &entry));
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "/path/to/binary");

    GtkWidget *btn = hv_make_action_btn("View Hex Dump");
    GtkWidget *tv;
    GtkWidget *sw = hv_make_text_view(&tv, FALSE);

    HexViewCtx *ctx = g_new0(HexViewCtx, 1);
    ctx->entry = entry; ctx->tv = tv;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_hex_view), ctx);
    g_signal_connect(entry, "activate", G_CALLBACK(on_hex_view), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);

    GtkWidget *note = gtk_label_new("Shows first 100 lines of hex dump (xxd)");
    gtk_widget_add_css_class(note, "helvetia-fg-muted");
    gtk_widget_set_halign(note, GTK_ALIGN_START);

    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), note);
    gtk_box_append(GTK_BOX(box), sw);
    return box;
}
