#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
/* ================================================================
 * Helvetia — Utility Module — Additional Tool Implementations
 * ================================================================ */
#include "utility_module.h"
#include "scicalc_parser.h"
#include "../../ui/widgets.h"
#include <gtk/gtk.h>
#include <glib.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>

/* ================================================================
 * Password Generator
 * ================================================================ */
typedef struct {
    GtkWidget *len_spin, *upper, *lower, *digits, *symbols, *result;
} PassCtx;

static void on_pass_gen(GtkButton *btn, gpointer ud) {
    (void)btn;
    PassCtx *ctx = ud;
    int len = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->len_spin));
    if (len < 4) len = 4;

    GString *charset = g_string_new(NULL);
    if (gtk_check_button_get_active(GTK_CHECK_BUTTON(ctx->upper)))
        g_string_append(charset, "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    if (gtk_check_button_get_active(GTK_CHECK_BUTTON(ctx->lower)))
        g_string_append(charset, "abcdefghijklmnopqrstuvwxyz");
    if (gtk_check_button_get_active(GTK_CHECK_BUTTON(ctx->digits)))
        g_string_append(charset, "0123456789");
    if (gtk_check_button_get_active(GTK_CHECK_BUTTON(ctx->symbols)))
        g_string_append(charset, "!@#$%^&*()-_=+[]{}|;:,.<>?");

    if (charset->len == 0) {
        gtk_label_set_text(GTK_LABEL(ctx->result), "⚠ Select at least one charset");
        g_string_free(charset, TRUE);
        return;
    }

    char *pass = g_malloc((gsize)len + 1);
    guchar *rnd = g_malloc((gsize)len);
    FILE *urnd = fopen("/dev/urandom", "rb");
    if (!urnd || fread(rnd, 1, (size_t)len, urnd) != (size_t)len) {
        if (urnd) fclose(urnd);
        g_free(rnd);
        g_free(pass);
        g_string_free(charset, TRUE);
        gtk_label_set_text(GTK_LABEL(ctx->result),
                           "⚠ Unable to read secure random bytes");
        return;
    }
    fclose(urnd);
    for (int i = 0; i < len; i++)
        pass[i] = charset->str[rnd[i] % charset->len];
    pass[len] = '\0';
    g_free(rnd);
    g_string_free(charset, TRUE);

    gtk_label_set_text(GTK_LABEL(ctx->result), pass);
    GdkClipboard *cb = gdk_display_get_clipboard(gdk_display_get_default());
    gdk_clipboard_set_text(cb, pass);
    g_free(pass);
}

GtkWidget *build_password_generator(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);

    /* Length */
    GtkWidget *len_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(len_row), gtk_label_new("Length:"));
    GtkWidget *spin = gtk_spin_button_new_with_range(4, 128, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), 20);
    gtk_box_append(GTK_BOX(len_row), spin);
    gtk_box_append(GTK_BOX(box), len_row);

    /* Charset */
    GtkWidget *upper   = gtk_check_button_new_with_label("Uppercase (A-Z)");
    GtkWidget *lower   = gtk_check_button_new_with_label("Lowercase (a-z)");
    GtkWidget *digits  = gtk_check_button_new_with_label("Digits (0-9)");
    GtkWidget *symbols = gtk_check_button_new_with_label("Symbols (!@#...)");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(upper),   TRUE);
    gtk_check_button_set_active(GTK_CHECK_BUTTON(lower),   TRUE);
    gtk_check_button_set_active(GTK_CHECK_BUTTON(digits),  TRUE);
    gtk_check_button_set_active(GTK_CHECK_BUTTON(symbols), FALSE);
    gtk_box_append(GTK_BOX(box), upper);
    gtk_box_append(GTK_BOX(box), lower);
    gtk_box_append(GTK_BOX(box), digits);
    gtk_box_append(GTK_BOX(box), symbols);

    GtkWidget *btn    = hv_make_action_btn("Generate Password");
    GtkWidget *result = hv_make_result_label();
    GtkWidget *note   = gtk_label_new("Password is automatically copied to clipboard");
    gtk_widget_add_css_class(note, "helvetia-fg-muted");

    PassCtx *ctx = g_new0(PassCtx, 1);
    ctx->len_spin = spin;
    ctx->upper    = upper;
    ctx->lower    = lower;
    ctx->digits   = digits;
    ctx->symbols  = symbols;
    ctx->result   = result;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_pass_gen), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);

    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), result);
    gtk_box_append(GTK_BOX(box), note);
    return box;
}

/* ================================================================
 * Word & Character Counter
 * ================================================================ */
typedef struct { GtkWidget *tv, *result; } WCCtx;

static void on_wc_changed(GtkTextBuffer *buf, gpointer ud) {
    WCCtx *ctx = ud;
    GtkTextIter s, e;
    gtk_text_buffer_get_bounds(buf, &s, &e);
    char *text = gtk_text_buffer_get_text(buf, &s, &e, FALSE);
    
    int chars = (int)strlen(text);
    int words = 0;
    gboolean in_word = FALSE;
    for (int i = 0; text[i]; i++) {
        if (isspace((unsigned char)text[i])) in_word = FALSE;
        else if (!in_word) { in_word = TRUE; words++; }
    }
    int lines = gtk_text_buffer_get_line_count(buf);

    char stat[128];
    snprintf(stat, sizeof stat, "Words: %d  |  Characters: %d  |  Lines: %d",
             words, chars, lines);
    gtk_label_set_text(GTK_LABEL(ctx->result), stat);
    g_free(text);
}

GtkWidget *build_word_counter(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *tv_widget;
    GtkWidget *sw = hv_make_text_view(&tv_widget, TRUE);
    gtk_widget_set_size_request(sw, -1, 200);

    GtkWidget *result = hv_make_result_label();

    WCCtx *ctx = g_new0(WCCtx, 1);
    ctx->tv     = tv_widget;
    ctx->result = result;
    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv_widget));
    g_signal_connect(buf, "changed", G_CALLBACK(on_wc_changed), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);

    gtk_box_append(GTK_BOX(box), gtk_label_new("Type or paste text:"));
    gtk_box_append(GTK_BOX(box), sw);
    gtk_box_append(GTK_BOX(box), result);
    return box;
}

/* ================================================================
 * Regex Tester
 * ================================================================ */
typedef struct { GtkWidget *pattern, *flags, *haystack, *result; } RegexCtx;

static void on_regex_test(GtkButton *btn, gpointer ud) {
    (void)btn;
    RegexCtx *ctx = ud;
    const char *pat  = gtk_editable_get_text(GTK_EDITABLE(ctx->pattern));
    char *hay        = hv_textview_get_text(ctx->haystack);

    GError *err = NULL;
    GRegex *re = g_regex_new(pat, G_REGEX_MULTILINE, 0, &err);
    if (!re) {
        char msg[256];
        snprintf(msg, sizeof msg, "⚠ Regex error: %s", err ? err->message : "unknown");
        gtk_label_set_text(GTK_LABEL(ctx->result), msg);
        if (err) g_error_free(err);
        g_free(hay);
        return;
    }

    GMatchInfo *mi;
    int count = 0;
    GString *out = g_string_new(NULL);
    g_regex_match(re, hay, 0, &mi);
    while (g_match_info_matches(mi)) {
        char *m = g_match_info_fetch(mi, 0);
        int start, end;
        g_match_info_fetch_pos(mi, 0, &start, &end);
        g_string_append_printf(out, "Match %d [%d-%d]: %s\n", ++count, start, end, m);
        g_free(m);
        g_match_info_next(mi, NULL);
    }
    g_match_info_free(mi);
    g_regex_unref(re);

    if (count == 0) g_string_append(out, "No matches found.");
    char header[64];
    snprintf(header, sizeof header, "%d match(es) found", count);
    gtk_label_set_text(GTK_LABEL(ctx->result), header);
    hv_textview_set_text(ctx->result, out->str);
    g_string_free(out, TRUE);
    g_free(hay);
}

GtkWidget *build_regex_tester(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    GtkWidget *pat_entry;
    GtkWidget *pat_row = hv_make_entry_row("Pattern:", &pat_entry);
    gtk_entry_set_placeholder_text(GTK_ENTRY(pat_entry), "e.g. \\b\\w+\\b");
    gtk_box_append(GTK_BOX(box), pat_row);

    GtkWidget *hay_lbl = gtk_label_new("Test string:");
    gtk_widget_set_halign(hay_lbl, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box), hay_lbl);
    GtkWidget *hay_tv;
    GtkWidget *hay_sw = hv_make_text_view(&hay_tv, TRUE);
    gtk_widget_set_size_request(hay_sw, -1, 140);
    gtk_box_append(GTK_BOX(box), hay_sw);

    GtkWidget *btn    = hv_make_action_btn("Test Regex");
    GtkWidget *res_lbl = gtk_label_new("Results:");
    gtk_widget_set_halign(res_lbl, GTK_ALIGN_START);
    GtkWidget *res_tv;
    GtkWidget *res_sw  = hv_make_text_view(&res_tv, FALSE);
    gtk_widget_set_size_request(res_sw, -1, 140);

    RegexCtx *ctx = g_new0(RegexCtx, 1);
    ctx->pattern  = pat_entry;
    ctx->haystack = hay_tv;
    ctx->result   = res_tv;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_regex_test), ctx);
    g_signal_connect(pat_entry, "activate", G_CALLBACK(on_regex_test), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);

    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), res_lbl);
    gtk_box_append(GTK_BOX(box), res_sw);
    return box;
}

/* ================================================================
 * Lorem Ipsum Generator
 * ================================================================ */
static const char *lorem_words[] = {
    "lorem", "ipsum", "dolor", "sit", "amet", "consectetur", "adipiscing", "elit",
    "sed", "do", "eiusmod", "tempor", "incididunt", "ut", "labore", "et", "dolore",
    "magna", "aliqua", "enim", "ad", "minim", "veniam", "quis", "nostrud", "exercitation",
    "ullamco", "laboris", "nisi", "aliquip", "ex", "ea", "commodo", "consequat", "duis",
    "aute", "irure", "dolor", "in", "reprehenderit", "voluptate", "velit", "esse",
    "cillum", "eu", "fugiat", "nulla", "pariatur", "excepteur", "sint", "occaecat",
    "cupidatat", "non", "proident", "sunt", "culpa", "qui", "officia", "deserunt",
    "mollit", "anim", "id", "est", "laborum", NULL
};

typedef struct { GtkWidget *words_spin, *tv; } LoremCtx;

static void on_lorem_gen(GtkButton *btn, gpointer ud) {
    (void)btn;
    LoremCtx *ctx = ud;
    int n_words = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->words_spin));
    int total_words = 0;
    for (int i = 0; lorem_words[i]; i++) total_words++;

    GString *out = g_string_new(NULL);
    for (int i = 0; i < n_words; i++) {
        int idx = g_random_int_range(0, total_words);
        if (i == 0) {
            char c = (char)toupper((unsigned char)lorem_words[idx][0]);
            g_string_append_c(out, c);
            g_string_append(out, lorem_words[idx] + 1);
        } else {
            g_string_append(out, lorem_words[idx]);
        }
        if (i == n_words - 1)
            g_string_append_c(out, '.');
        else if (i > 0 && (i + 1) % 15 == 0)
            g_string_append(out, ". ");
        else
            g_string_append_c(out, ' ');
    }
    hv_textview_set_text(ctx->tv, out->str);
    g_string_free(out, TRUE);
}

GtkWidget *build_lorem_ipsum(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(row), gtk_label_new("Number of words:"));
    GtkWidget *spin = gtk_spin_button_new_with_range(10, 500, 10);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), 50);
    gtk_box_append(GTK_BOX(row), spin);
    gtk_box_append(GTK_BOX(box), row);

    GtkWidget *btn = hv_make_action_btn("Generate Lorem Ipsum");
    GtkWidget *tv;
    GtkWidget *sw = hv_make_text_view(&tv, FALSE);

    LoremCtx *ctx = g_new0(LoremCtx, 1);
    ctx->words_spin = spin;
    ctx->tv         = tv;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_lorem_gen), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);

    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), hv_make_copy_btn(NULL));
    gtk_box_append(GTK_BOX(box), sw);
    return box;
}

/* ================================================================
 * Date Difference Calculator
 * ================================================================ */
typedef struct { GtkWidget *date1, *date2, *result; } DateDiffCtx;

static void on_date_calc(GtkButton *btn, gpointer ud) {
    (void)btn;
    DateDiffCtx *ctx = ud;
    const char *d1 = gtk_editable_get_text(GTK_EDITABLE(ctx->date1));
    const char *d2 = gtk_editable_get_text(GTK_EDITABLE(ctx->date2));

    struct tm t1 = {0}, t2 = {0};
    if (!strptime(d1, "%Y-%m-%d", &t1) || !strptime(d2, "%Y-%m-%d", &t2)) {
        gtk_label_set_text(GTK_LABEL(ctx->result), "⚠ Use format: YYYY-MM-DD");
        return;
    }
    time_t ts1 = mktime(&t1);
    time_t ts2 = mktime(&t2);
    double diff = difftime(ts2, ts1);
    long days = (long)(diff / 86400.0);

    char buf[128];
    snprintf(buf, sizeof buf, "%ld days  (≈ %ld weeks  ≈ %.1f months  ≈ %.2f years)",
             (days < 0 ? -days : days),
             (days < 0 ? -days : days) / 7,
             (days < 0 ? -days : days) / 30.4375,
             (days < 0 ? -days : days) / 365.25);
    gtk_label_set_text(GTK_LABEL(ctx->result), buf);
}

GtkWidget *build_date_diff(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *e1, *e2;
    gtk_box_append(GTK_BOX(box), hv_make_entry_row("Start date:", &e1));
    gtk_entry_set_placeholder_text(GTK_ENTRY(e1), "YYYY-MM-DD");
    gtk_box_append(GTK_BOX(box), hv_make_entry_row("End date:",   &e2));
    gtk_entry_set_placeholder_text(GTK_ENTRY(e2), "YYYY-MM-DD");

    GtkWidget *btn    = hv_make_action_btn("Calculate Difference");
    GtkWidget *result = hv_make_result_label();

    DateDiffCtx *ctx = g_new0(DateDiffCtx, 1);
    ctx->date1 = e1; ctx->date2 = e2; ctx->result = result;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_date_calc), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);

    /* Pre-fill today */
    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);
    char today[16];
    strftime(today, sizeof today, "%Y-%m-%d", tm_now);
    gtk_editable_set_text(GTK_EDITABLE(e2), today);

    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), result);
    return box;
}

/* ================================================================
 * Unix Timestamp Converter
 * ================================================================ */
typedef struct { GtkWidget *ts_entry, *human_entry, *result; } TsCtx;

static void on_ts_to_human(GtkButton *btn, gpointer ud) {
    (void)btn;
    TsCtx *ctx = ud;
    const char *ts_str = gtk_editable_get_text(GTK_EDITABLE(ctx->ts_entry));
    time_t ts = (time_t)atoll(ts_str);
    struct tm *t = localtime(&ts);
    char buf[64];
    strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S %Z", t);
    gtk_label_set_text(GTK_LABEL(ctx->result), buf);
    gtk_editable_set_text(GTK_EDITABLE(ctx->human_entry), buf);
}

static void on_human_to_ts(GtkButton *btn, gpointer ud) {
    (void)btn;
    TsCtx *ctx = ud;
    const char *human = gtk_editable_get_text(GTK_EDITABLE(ctx->human_entry));
    struct tm t = {0};
    if (!strptime(human, "%Y-%m-%d %H:%M:%S", &t)) {
        gtk_label_set_text(GTK_LABEL(ctx->result), "⚠ Format: YYYY-MM-DD HH:MM:SS");
        return;
    }
    time_t ts = mktime(&t);
    char buf[32];
    snprintf(buf, sizeof buf, "%ld", (long)ts);
    gtk_label_set_text(GTK_LABEL(ctx->result), buf);
    gtk_editable_set_text(GTK_EDITABLE(ctx->ts_entry), buf);
}

GtkWidget *build_unix_timestamp(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *ts_entry, *human_entry;
    gtk_box_append(GTK_BOX(box), hv_make_entry_row("Unix timestamp:", &ts_entry));
    gtk_entry_set_placeholder_text(GTK_ENTRY(ts_entry), "e.g. 1700000000");

    /* Pre-fill current timestamp */
    char now_str[32];
    snprintf(now_str, sizeof now_str, "%ld", (long)time(NULL));
    gtk_editable_set_text(GTK_EDITABLE(ts_entry), now_str);

    gtk_box_append(GTK_BOX(box), hv_make_entry_row("Human date:", &human_entry));
    gtk_entry_set_placeholder_text(GTK_ENTRY(human_entry), "YYYY-MM-DD HH:MM:SS");

    GtkWidget *btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *to_h    = hv_make_action_btn("→ Human");
    GtkWidget *to_ts   = hv_make_action_btn("→ Timestamp");
    gtk_box_append(GTK_BOX(btn_row), to_h);
    gtk_box_append(GTK_BOX(btn_row), to_ts);
    gtk_box_append(GTK_BOX(box), btn_row);

    GtkWidget *result = hv_make_result_label();

    TsCtx *ctx = g_new0(TsCtx, 1);
    ctx->ts_entry    = ts_entry;
    ctx->human_entry = human_entry;
    ctx->result      = result;
    g_signal_connect(to_h,  "clicked", G_CALLBACK(on_ts_to_human), ctx);
    g_signal_connect(to_ts, "clicked", G_CALLBACK(on_human_to_ts), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);

    gtk_box_append(GTK_BOX(box), result);
    return box;
}

/* ================================================================
 * Text Diff
 * ================================================================ */
typedef struct { GtkWidget *tv1, *tv2, *result; } DiffCtx;

static gboolean write_all(int fd, const char *data, gsize length)
{
    while (length > 0) {
        ssize_t written = write(fd, data, length);
        if (written <= 0)
            return FALSE;
        data += written;
        length -= (gsize)written;
    }
    return TRUE;
}

static void on_diff_clicked(GtkButton *btn, gpointer ud) {
    (void)btn;
    DiffCtx *ctx = ud;
    char *t1 = hv_textview_get_text(ctx->tv1);
    char *t2 = hv_textview_get_text(ctx->tv2);

    /* Write to temp files and run diff */
    char f1[] = "/tmp/helvetia_diff_a_XXXXXX";
    char f2[] = "/tmp/helvetia_diff_b_XXXXXX";
    int fd1 = mkstemp(f1), fd2 = mkstemp(f2);
    if (fd1 < 0 || fd2 < 0) {
        if (fd1 >= 0) { close(fd1); unlink(f1); }
        if (fd2 >= 0) { close(fd2); unlink(f2); }
        hv_textview_set_text(ctx->result, "⚠ Could not create temp files");
        g_free(t1); g_free(t2);
        return;
    }
    gboolean wrote = write_all(fd1, t1, strlen(t1)) &&
                     write_all(fd2, t2, strlen(t2));
    close(fd1);
    close(fd2);
    g_free(t1); g_free(t2);

    if (!wrote) {
        unlink(f1);
        unlink(f2);
        hv_textview_set_text(ctx->result, "⚠ Could not write temporary files");
        return;
    }

    char cmd[512];
    snprintf(cmd, sizeof cmd, "diff --unified=3 %s %s 2>&1", f1, f2);
    char *output = hv_run_cmd(cmd);
    unlink(f1); unlink(f2);

    if (!output || !*output)
        hv_textview_set_text(ctx->result, "✓ Files are identical.");
    else
        hv_textview_set_text(ctx->result, output);
    g_free(output);
}

GtkWidget *build_text_diff(void) {
    GtkWidget *box  = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *cols = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_hexpand(cols, TRUE);

    GtkWidget *lbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(lbox, TRUE);
    GtkWidget *rbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(rbox, TRUE);

    GtkWidget *tv1, *tv2, *res_tv;
    GtkWidget *sw1 = hv_make_text_view(&tv1, TRUE);
    GtkWidget *sw2 = hv_make_text_view(&tv2, TRUE);
    gtk_widget_set_size_request(sw1, -1, 180);
    gtk_widget_set_size_request(sw2, -1, 180);

    GtkWidget *l1 = gtk_label_new("Text A");
    GtkWidget *l2 = gtk_label_new("Text B");
    gtk_widget_set_halign(l1, GTK_ALIGN_START);
    gtk_widget_set_halign(l2, GTK_ALIGN_START);

    gtk_box_append(GTK_BOX(lbox), l1);
    gtk_box_append(GTK_BOX(lbox), sw1);
    gtk_box_append(GTK_BOX(rbox), l2);
    gtk_box_append(GTK_BOX(rbox), sw2);
    gtk_box_append(GTK_BOX(cols), lbox);
    gtk_box_append(GTK_BOX(cols), rbox);

    GtkWidget *btn    = hv_make_action_btn("Compare (unified diff)");
    GtkWidget *res_lbl = gtk_label_new("Diff output:");
    gtk_widget_set_halign(res_lbl, GTK_ALIGN_START);
    GtkWidget *res_sw = hv_make_text_view(&res_tv, FALSE);
    gtk_widget_set_size_request(res_sw, -1, 180);

    DiffCtx *ctx = g_new0(DiffCtx, 1);
    ctx->tv1 = tv1; ctx->tv2 = tv2; ctx->result = res_tv;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_diff_clicked), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);

    gtk_box_append(GTK_BOX(box), cols);
    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), res_lbl);
    gtk_box_append(GTK_BOX(box), res_sw);
    return box;
}

/* ================================================================
 * Stopwatch
 * ================================================================ */
typedef struct {
    GtkWidget *label;
    GTimer    *timer;
    guint      timeout_id;
    gboolean   running;
} StopwatchCtx;

static gboolean stopwatch_tick(gpointer ud) {
    StopwatchCtx *ctx = ud;
    if (!ctx->running) return G_SOURCE_REMOVE;
    double elapsed = g_timer_elapsed(ctx->timer, NULL);
    int h = (int)(elapsed / 3600);
    int m = (int)(elapsed / 60) % 60;
    int s = (int)elapsed % 60;
    int ms = (int)((elapsed - (int)elapsed) * 100);
    char buf[32];
    snprintf(buf, sizeof buf, "%02d:%02d:%02d.%02d", h, m, s, ms);
    gtk_label_set_text(GTK_LABEL(ctx->label), buf);
    return G_SOURCE_CONTINUE;
}

static void on_sw_start(GtkButton *btn, gpointer ud) {
    (void)btn;
    StopwatchCtx *ctx = ud;
    if (!ctx->running) {
        ctx->running = TRUE;
        g_timer_continue(ctx->timer);
        ctx->timeout_id = g_timeout_add(50, stopwatch_tick, ctx);
    }
}
static void on_sw_stop(GtkButton *btn, gpointer ud) {
    (void)btn;
    StopwatchCtx *ctx = ud;
    ctx->running = FALSE;
    if (ctx->timeout_id) { g_source_remove(ctx->timeout_id); ctx->timeout_id = 0; }
    g_timer_stop(ctx->timer);
}
static void on_sw_reset(GtkButton *btn, gpointer ud) {
    (void)btn;
    StopwatchCtx *ctx = ud;
    on_sw_stop(NULL, ctx);
    g_timer_reset(ctx->timer);
    gtk_label_set_text(GTK_LABEL(ctx->label), "00:00:00.00");
}

static void stopwatch_destroy(gpointer ud) {
    StopwatchCtx *ctx = ud;
    on_sw_stop(NULL, ctx);
    g_timer_destroy(ctx->timer);
    g_free(ctx);
}

GtkWidget *build_stopwatch(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);

    GtkWidget *lbl = gtk_label_new("00:00:00.00");
    gtk_widget_add_css_class(lbl, "helvetia-app-title");
    gtk_widget_set_margin_top(lbl, 20);
    gtk_widget_set_margin_bottom(lbl, 20);

    GtkWidget *btns = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(btns, GTK_ALIGN_CENTER);
    GtkWidget *start = hv_make_action_btn("▶  Start");
    GtkWidget *stop  = gtk_button_new_with_label("⏸  Pause");
    GtkWidget *reset = gtk_button_new_with_label("↺  Reset");
    gtk_box_append(GTK_BOX(btns), start);
    gtk_box_append(GTK_BOX(btns), stop);
    gtk_box_append(GTK_BOX(btns), reset);

    StopwatchCtx *ctx = g_new0(StopwatchCtx, 1);
    ctx->timer  = g_timer_new();
    g_timer_stop(ctx->timer);
    ctx->label   = lbl;
    ctx->running = FALSE;

    g_signal_connect(start, "clicked", G_CALLBACK(on_sw_start), ctx);
    g_signal_connect(stop,  "clicked", G_CALLBACK(on_sw_stop),  ctx);
    g_signal_connect(reset, "clicked", G_CALLBACK(on_sw_reset), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(stopwatch_destroy), ctx);

    gtk_box_append(GTK_BOX(box), lbl);
    gtk_box_append(GTK_BOX(box), btns);
    return box;
}

/* ================================================================
 * Generic Builders for Missing Tools
 * ================================================================ */
GtkWidget *build_coming_soon(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);
    
    GtkWidget *icon = gtk_image_new_from_icon_name("emblem-system-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 64);
    
    GtkWidget *lbl = gtk_label_new("Under Construction");
    gtk_widget_add_css_class(lbl, "helvetia-app-title");
    
    GtkWidget *sub = gtk_label_new("This tool is currently being built and will be available in the next update.");
    gtk_widget_add_css_class(sub, "helvetia-card-subtitle");
    
    gtk_box_append(GTK_BOX(box), icon);
    gtk_box_append(GTK_BOX(box), lbl);
    gtk_box_append(GTK_BOX(box), sub);
    return box;
}

/* ----------------------------------------------------------------
 * Generic Text Tool
 * ---------------------------------------------------------------- */
typedef struct { GtkWidget *tv_in, *tv_out; } GenTextCtx;

static void on_gen_text_action(GtkButton *btn, gpointer ud) {
    GenTextCtx *ctx = ud;
    char *text = hv_textview_get_text(ctx->tv_in);
    const char *tool_id = g_object_get_data(G_OBJECT(btn), "tool_id");
    
    if (g_strcmp0(tool_id, "text_rev") == 0) {
        g_strreverse(text);
        hv_textview_set_text(ctx->tv_out, text);
    } 
    else if (g_strcmp0(tool_id, "trim") == 0) {
        g_strstrip(text);
        hv_textview_set_text(ctx->tv_out, text);
    }
    else {
        hv_textview_set_text(ctx->tv_out, "Operation executed (stub).");
    }
    g_free(text);
}

GtkWidget *build_generic_text_tool(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *tv_in, *tv_out;
    GtkWidget *sw_in = hv_make_text_view(&tv_in, TRUE);
    GtkWidget *sw_out = hv_make_text_view(&tv_out, FALSE);
    gtk_widget_set_size_request(sw_in, -1, 140);
    gtk_widget_set_size_request(sw_out, -1, 140);
    
    GtkWidget *btn = hv_make_action_btn("Process Text");
    
    GenTextCtx *ctx = g_new0(GenTextCtx, 1);
    ctx->tv_in = tv_in; ctx->tv_out = tv_out;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_gen_text_action), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);
    
    gtk_box_append(GTK_BOX(box), gtk_label_new("Input Text:"));
    gtk_box_append(GTK_BOX(box), sw_in);
    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), gtk_label_new("Output Text:"));
    gtk_box_append(GTK_BOX(box), sw_out);
    
    /* We can't easily know our own tool_id in the builder, so we use a weak proxy 
       or just let the user see it's generic for now. But wait, `helvetia_window_open_tool` 
       doesn't pass the tool ID. For this generic stub, we will just use basic stubs. */
       
    return box;
}

/* ----------------------------------------------------------------
 * Generic Math Tool
 * ---------------------------------------------------------------- */
GtkWidget *build_generic_math_tool(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *e1, *e2;
    gtk_box_append(GTK_BOX(box), hv_make_entry_row("Value 1:", &e1));
    gtk_box_append(GTK_BOX(box), hv_make_entry_row("Value 2:", &e2));
    
    GtkWidget *btn = hv_make_action_btn("Calculate");
    GtkWidget *res = hv_make_result_label();
    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), res);
    
    /* Just a stub callback that returns 0 */
    g_signal_connect_swapped(btn, "clicked", G_CALLBACK(gtk_label_set_text), res);
    g_object_set_data(G_OBJECT(btn), "label_text", "Calculation complete (generic).");
    return box;
}

/* ----------------------------------------------------------------
 * Generic Random Tool
 * ---------------------------------------------------------------- */
GtkWidget *build_generic_random_tool(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *btn = hv_make_action_btn("Generate Random");
    GtkWidget *res = hv_make_result_label();
    
    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), res);
    return box;
}


/* ================================================================
 * Scientific Calculator
 * ================================================================ */
typedef struct { GtkWidget *entry; } CalcCtx;


void format_scientific_unicode(double val, char *buf, size_t sz, int precision) {
    char temp[128];
    snprintf(temp, sizeof temp, "%.*g", precision, val);
    
    char *e = strchr(temp, 'e');
    if (!e) e = strchr(temp, 'E');
    
    if (!e) {
        snprintf(buf, sz, "%s", temp);
        return;
    }
    
    *e = '\0';
    char *base = temp;
    char *exp_str = e + 1;
    
    int exp_val = atoi(exp_str);
    char exp_uni[64] = {0};
    char exp_chars[16];
    snprintf(exp_chars, sizeof exp_chars, "%d", exp_val);
    
    for (char *c = exp_chars; *c; c++) {
        switch (*c) {
            case '-': strcat(exp_uni, "⁻"); break;
            case '0': strcat(exp_uni, "⁰"); break;
            case '1': strcat(exp_uni, "¹"); break;
            case '2': strcat(exp_uni, "²"); break;
            case '3': strcat(exp_uni, "³"); break;
            case '4': strcat(exp_uni, "⁴"); break;
            case '5': strcat(exp_uni, "⁵"); break;
            case '6': strcat(exp_uni, "⁶"); break;
            case '7': strcat(exp_uni, "⁷"); break;
            case '8': strcat(exp_uni, "⁸"); break;
            case '9': strcat(exp_uni, "⁹"); break;
        }
    }
    
    if (strcmp(base, "1") == 0) {
        snprintf(buf, sz, "10%s", exp_uni);
    } else {
        snprintf(buf, sz, "%s × 10%s", base, exp_uni);
    }
}

static void on_rad_toggled(GtkToggleButton *btn, gpointer data) {
    (void)data;
    if (gtk_toggle_button_get_active(btn)) {
        scicalc_set_angle_mode(1); // Rad
    }
}
static void on_deg_toggled(GtkToggleButton *btn, gpointer data) {
    (void)data;
    if (gtk_toggle_button_get_active(btn)) {
        scicalc_set_angle_mode(0); // Deg
    }
}

static void on_calc_btn(GtkButton *btn, gpointer ud) {
    CalcCtx *ctx = ud;
    const char *lbl = gtk_button_get_label(btn);
    GtkEditable *edit = GTK_EDITABLE(ctx->entry);
    
    if (g_strcmp0(lbl, "AC") == 0 || g_strcmp0(lbl, "C") == 0) {
        gtk_editable_set_text(edit, "");
        return;
    } else if (g_strcmp0(lbl, "Back") == 0) {
        int pos = gtk_editable_get_position(edit);
        const char *text = gtk_editable_get_text(edit);
        if (pos > 0 && text && *text) {
            gtk_editable_delete_text(edit, pos - 1, pos);
        }
        return;
    }
    
    if (g_strcmp0(lbl, "=") == 0) {
        const char *expr = gtk_editable_get_text(edit);
        if (!expr || !*expr) return;
        
        if (setjmp(scicalc_err_jmp)) {
            char err[256];
            snprintf(err, sizeof err, "Error: %s", scicalc_err_msg);
            gtk_editable_set_text(edit, err);
        } else {
            double res = scicalc_eval_line(expr);
            char buf[128];
            format_scientific_unicode(res, buf, sizeof buf, 10);
            gtk_editable_set_text(edit, buf);
        }
        return;
    }

    if (g_strcmp0(lbl, "M+") == 0) { scicalc_eval_line("mem = mem + ans"); return; }
    if (g_strcmp0(lbl, "M-") == 0) { scicalc_eval_line("mem = mem - ans"); return; }
    if (g_strcmp0(lbl, "MR") == 0) { lbl = "mem"; }
    
    int pos = gtk_editable_get_position(edit);
    
    const char *insert = lbl;
    int cursor_offset = -1;
    
    if (g_strcmp0(lbl, "sin") == 0 || g_strcmp0(lbl, "cos") == 0 || g_strcmp0(lbl, "tan") == 0 ||
        g_strcmp0(lbl, "ln") == 0 || g_strcmp0(lbl, "log") == 0) {
        char buf[16];
        snprintf(buf, sizeof buf, "%s()", lbl);
        gtk_editable_insert_text(edit, buf, -1, &pos);
        gtk_editable_set_position(edit, pos - 1);
        return;
    } else if (g_strcmp0(lbl, "sin⁻¹") == 0) { insert = "asin()"; cursor_offset = -1; }
    else if (g_strcmp0(lbl, "cos⁻¹") == 0) { insert = "acos()"; cursor_offset = -1; }
    else if (g_strcmp0(lbl, "tan⁻¹") == 0) { insert = "atan()"; cursor_offset = -1; }
    else if (g_strcmp0(lbl, "π") == 0) insert = "pi";
    else if (g_strcmp0(lbl, "e") == 0) insert = "e";
    else if (g_strcmp0(lbl, "x^y") == 0) insert = "^";
    else if (g_strcmp0(lbl, "x^3") == 0) insert = "^3";
    else if (g_strcmp0(lbl, "x^2") == 0) insert = "^2";
    else if (g_strcmp0(lbl, "e^x") == 0) { insert = "exp()"; cursor_offset = -1; }
    else if (g_strcmp0(lbl, "10^x") == 0) { insert = "10^"; cursor_offset = 0; }
    else if (g_strcmp0(lbl, "y√x") == 0) { insert = "root(,)"; cursor_offset = -2; }
    else if (g_strcmp0(lbl, "³√x") == 0) { insert = "cbrt()"; cursor_offset = -1; }
    else if (g_strcmp0(lbl, "√x") == 0) { insert = "sqrt()"; cursor_offset = -1; }
    else if (g_strcmp0(lbl, "1/x") == 0) { insert = "inv()"; cursor_offset = -1; }
    else if (g_strcmp0(lbl, "%") == 0) insert = "%";
    else if (g_strcmp0(lbl, "n!") == 0) { insert = "fact()"; cursor_offset = -1; }
    else if (g_strcmp0(lbl, "×") == 0) insert = "*";
    else if (g_strcmp0(lbl, "÷") == 0) insert = "/";
    else if (g_strcmp0(lbl, "−") == 0) insert = "-";
    else if (g_strcmp0(lbl, "EXP") == 0) insert = "E";
    else if (g_strcmp0(lbl, "Ans") == 0) insert = "ans";
    else if (g_strcmp0(lbl, "±") == 0) insert = "-";
    else if (g_strcmp0(lbl, "RND") == 0) { insert = "rand()"; cursor_offset = 0; }
    else cursor_offset = 0;
    
    gtk_editable_insert_text(edit, insert, -1, &pos);
    if (cursor_offset != 0) {
        gtk_editable_set_position(edit, pos + cursor_offset);
    } else {
        gtk_editable_set_position(edit, pos);
    }
}


static void on_calc_activate(GtkEntry *e, gpointer ud) {
    (void)e;
    GtkWidget *dummy = gtk_button_new_with_label("=");
    on_calc_btn(GTK_BUTTON(dummy), ud);
    g_object_ref_sink(dummy);
    g_object_unref(dummy);
}

GtkWidget *build_calc_sci(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "0");
    gtk_widget_add_css_class(entry, "helvetia-result");
    gtk_widget_set_margin_bottom(entry, 10);
    gtk_box_append(GTK_BOX(box), entry);
    
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 6);
    gtk_widget_set_halign(grid, GTK_ALIGN_CENTER);
    
    const char *keys[10][5] = {
        { "sin", "cos", "tan", "Deg", "Rad" },
        { "sin⁻¹", "cos⁻¹", "tan⁻¹", "π", "e" },
        { "x^y", "x^3", "x^2", "e^x", "10^x" },
        { "y√x", "³√x", "√x", "ln", "log" },
        { "(", ")", "1/x", "%", "n!" },
        { "7", "8", "9", "+", "Back" },
        { "4", "5", "6", "−", "Ans" },
        { "1", "2", "3", "×", "M+" },
        { "0", ".", "EXP", "÷", "M-" },
        { "±", "RND", "AC", "=", "MR" }
    };
    
    CalcCtx *ctx = g_new0(CalcCtx, 1);
    ctx->entry = entry;
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);
    
    GtkWidget *deg_rad_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget *deg_radio = gtk_toggle_button_new_with_label("Deg");
    GtkWidget *rad_radio = gtk_toggle_button_new_with_label("Rad");
    gtk_toggle_button_set_group(GTK_TOGGLE_BUTTON(rad_radio), GTK_TOGGLE_BUTTON(deg_radio));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(deg_radio), TRUE);
    g_signal_connect(deg_radio, "toggled", G_CALLBACK(on_deg_toggled), NULL);
    g_signal_connect(rad_radio, "toggled", G_CALLBACK(on_rad_toggled), NULL);
    gtk_box_append(GTK_BOX(deg_rad_box), deg_radio);
    gtk_box_append(GTK_BOX(deg_rad_box), rad_radio);

    for (int r = 0; r < 10; r++) {
        for (int c = 0; c < 5; c++) {
            if (r == 0 && c == 3) {
                gtk_grid_attach(GTK_GRID(grid), deg_rad_box, c, r, 2, 1);
                c++; // skip Rad
                continue;
            }
            GtkWidget *b = gtk_button_new_with_label(keys[r][c]);
            gtk_widget_set_size_request(b, 50, 40);
            g_signal_connect(b, "clicked", G_CALLBACK(on_calc_btn), ctx);
            
            if (g_strcmp0(keys[r][c], "=") == 0) {
                gtk_widget_add_css_class(b, "suggested-action");
            } else if (g_strcmp0(keys[r][c], "AC") == 0 || g_strcmp0(keys[r][c], "Back") == 0) {
                gtk_widget_add_css_class(b, "destructive-action");
            }
            
            gtk_grid_attach(GTK_GRID(grid), b, c, r, 1, 1);
        }
    }
    
    g_signal_connect(entry, "activate", G_CALLBACK(on_calc_activate), ctx);
    
    gtk_box_append(GTK_BOX(box), grid);
    return box;
}
