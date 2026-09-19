#define _POSIX_C_SOURCE 200809L
/* ================================================================
 * Helvetia — PDF Module — Tool Implementations (poppler-utils)
 * ================================================================ */
#include "pdf_module.h"
#include "../../ui/widgets.h"
#include <gtk/gtk.h>
#include <glib.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ================================================================
 * PDF to Text (pdftotext)
 * ================================================================ */
typedef struct { GtkWidget *in_e, *out_e, *status; } PdfCtx;

static void on_pdf_to_text(GtkButton *btn, gpointer ud) {
    (void)btn;
    PdfCtx *ctx = ud;
    const char *in = gtk_editable_get_text(GTK_EDITABLE(ctx->in_e));
    const char *out = gtk_editable_get_text(GTK_EDITABLE(ctx->out_e));
    if (!in || !*in || !out || !*out) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Fill both paths");
        return;
    }
    char *qi = g_shell_quote(in);
    char *qo = g_shell_quote(out);
    char *cmd = g_strdup_printf("pdftotext %s %s 2>&1", qi, qo);
    g_free(qi); g_free(qo);

    char *res = hv_run_cmd(cmd);
    g_free(cmd);
    if (!res || !*res)
        gtk_label_set_text(GTK_LABEL(ctx->status), "✓ PDF converted to text!");
    else
        gtk_label_set_text(GTK_LABEL(ctx->status), res);
    g_free(res);
}

GtkWidget *build_pdf_to_text(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *in_e, *out_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input PDF:", &in_e, FALSE));
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output TXT:", &out_e, TRUE));

    GtkWidget *btn = hv_make_action_btn("Convert to Text");
    GtkWidget *status = hv_make_result_label();
    
    PdfCtx *ctx = g_new0(PdfCtx, 1);
    ctx->in_e = in_e; ctx->out_e = out_e; ctx->status = status;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_pdf_to_text), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);

    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), status);
    return box;
}

/* ================================================================
 * PDF to Image (pdftocairo / pdftoppm)
 * ================================================================ */
static void on_pdf_to_image(GtkButton *btn, gpointer ud) {
    (void)btn;
    PdfCtx *ctx = ud;
    const char *in = gtk_editable_get_text(GTK_EDITABLE(ctx->in_e));
    const char *out = gtk_editable_get_text(GTK_EDITABLE(ctx->out_e)); /* acts as prefix */
    if (!in || !*in || !out || !*out) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Fill both paths");
        return;
    }
    char *qi = g_shell_quote(in);
    char *qo = g_shell_quote(out);
    /* -png outputs to prefix-01.png, prefix-02.png etc */
    char *cmd = g_strdup_printf("pdftocairo -png %s %s 2>&1", qi, qo);
    g_free(qi); g_free(qo);

    char *res = hv_run_cmd(cmd);
    g_free(cmd);
    if (!res || !*res)
        gtk_label_set_text(GTK_LABEL(ctx->status), "✓ PDF pages extracted to images!");
    else
        gtk_label_set_text(GTK_LABEL(ctx->status), res);
    g_free(res);
}

GtkWidget *build_pdf_to_image(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *in_e, *out_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input PDF:", &in_e, FALSE));
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output prefix:", &out_e, TRUE));
    gtk_entry_set_placeholder_text(GTK_ENTRY(out_e), "/path/to/output_image_prefix");

    GtkWidget *btn = hv_make_action_btn("Convert Pages to PNG");
    GtkWidget *status = hv_make_result_label();
    
    PdfCtx *ctx = g_new0(PdfCtx, 1);
    ctx->in_e = in_e; ctx->out_e = out_e; ctx->status = status;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_pdf_to_image), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);

    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), status);
    return box;
}

/* ================================================================
 * PDF Merge (pdfunite / ghostscript)
 * ================================================================ */
typedef struct { GtkWidget *tv_in, *out_e, *status; } PdfMergeCtx;

static void on_pdf_merge(GtkButton *btn, gpointer ud) {
    (void)btn;
    PdfMergeCtx *ctx = ud;
    char *in = hv_textview_get_text(ctx->tv_in);
    const char *out = gtk_editable_get_text(GTK_EDITABLE(ctx->out_e));
    if (!in || !*in || !out || !*out) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Enter input PDFs and output path");
        g_free(in);
        return;
    }
    
    /* Convert lines to space-separated quoted args */
    GString *args = g_string_new("");
    char **lines = g_strsplit(in, "\n", -1);
    for (int i = 0; lines[i]; i++) {
        g_strstrip(lines[i]);
        if (*lines[i]) {
            char *q = g_shell_quote(lines[i]);
            g_string_append(args, q);
            g_string_append_c(args, ' ');
            g_free(q);
        }
    }
    g_strfreev(lines);
    g_free(in);
    
    if (args->len == 0) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ No valid input files");
        g_string_free(args, TRUE);
        return;
    }

    char *qo = g_shell_quote(out);
    /* pdfunite in1 in2 out */
    char *cmd = g_strdup_printf("pdfunite %s %s 2>&1", args->str, qo);
    g_string_free(args, TRUE);
    g_free(qo);

    char *res = hv_run_cmd(cmd);
    g_free(cmd);
    if (!res || !*res)
        gtk_label_set_text(GTK_LABEL(ctx->status), "✓ PDFs merged successfully!");
    else
        gtk_label_set_text(GTK_LABEL(ctx->status), res);
    g_free(res);
}

GtkWidget *build_pdf_merge(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_box_append(GTK_BOX(box), gtk_label_new("Input PDFs (one per line):"));
    GtkWidget *tv;
    GtkWidget *sw = hv_make_text_view(&tv, TRUE);
    gtk_widget_set_size_request(sw, -1, 120);
    gtk_box_append(GTK_BOX(box), sw);

    GtkWidget *out_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output PDF:", &out_e, TRUE));
    
    GtkWidget *btn = hv_make_action_btn("Merge PDFs");
    GtkWidget *status = hv_make_result_label();
    
    PdfMergeCtx *ctx = g_new0(PdfMergeCtx, 1);
    ctx->tv_in = tv; ctx->out_e = out_e; ctx->status = status;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_pdf_merge), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);

    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), status);
    return box;
}

/* ================================================================
 * PDF Split (pdfseparate)
 * ================================================================ */
static void on_pdf_split(GtkButton *btn, gpointer ud) {
    (void)btn;
    PdfCtx *ctx = ud;
    const char *in = gtk_editable_get_text(GTK_EDITABLE(ctx->in_e));
    const char *out = gtk_editable_get_text(GTK_EDITABLE(ctx->out_e)); 
    if (!in || !*in || !out || !*out) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Fill both paths");
        return;
    }
    char *qi = g_shell_quote(in);
    char *qo = g_shell_quote(out);
    /* pdfseparate in out_pattern. e.g. out-%d.pdf */
    char *cmd = g_strdup_printf("pdfseparate %s %s 2>&1", qi, qo);
    g_free(qi); g_free(qo);

    char *res = hv_run_cmd(cmd);
    g_free(cmd);
    if (!res || !*res)
        gtk_label_set_text(GTK_LABEL(ctx->status), "✓ PDF split into pages!");
    else
        gtk_label_set_text(GTK_LABEL(ctx->status), res);
    g_free(res);
}

GtkWidget *build_pdf_split(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *in_e, *out_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input PDF:", &in_e, FALSE));
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output pattern:", &out_e, TRUE));
    gtk_entry_set_placeholder_text(GTK_ENTRY(out_e), "/path/to/page-%d.pdf");

    GtkWidget *btn = hv_make_action_btn("Split into Pages");
    GtkWidget *status = hv_make_result_label();
    
    PdfCtx *ctx = g_new0(PdfCtx, 1);
    ctx->in_e = in_e; ctx->out_e = out_e; ctx->status = status;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_pdf_split), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);

    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), status);
    return box;
}

/* ================================================================
 * PDF Extract Pages (qpdf)
 * ================================================================ */
typedef struct { GtkWidget *in_e, *out_e, *pages_e, *status; } PdfExtCtx;

static void on_pdf_extract(GtkButton *btn, gpointer ud) {
    (void)btn;
    PdfExtCtx *ctx = ud;
    const char *in = gtk_editable_get_text(GTK_EDITABLE(ctx->in_e));
    const char *out = gtk_editable_get_text(GTK_EDITABLE(ctx->out_e));
    const char *pages = gtk_editable_get_text(GTK_EDITABLE(ctx->pages_e));
    
    if (!in || !*in || !out || !*out || !pages || !*pages) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Fill all fields");
        return;
    }
    
    char *qi = g_shell_quote(in);
    char *qo = g_shell_quote(out);
    char *qp = g_shell_quote(pages);
    
    // qpdf --empty --pages input.pdf 1-5,7 -- output.pdf
    char *cmd = g_strdup_printf("qpdf --empty --pages %s %s -- %s 2>&1", qi, qp, qo);
    g_free(qi); g_free(qo); g_free(qp);

    char *res = hv_run_cmd(cmd);
    g_free(cmd);
    if (!res || !*res || strstr(res, "WARNING"))
        gtk_label_set_text(GTK_LABEL(ctx->status), "✓ Pages extracted successfully!");
    else
        gtk_label_set_text(GTK_LABEL(ctx->status), res);
    g_free(res);
}

GtkWidget *build_pdf_extract(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *in_e, *out_e, *pages_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input PDF:", &in_e, FALSE));
    
    GtkWidget *hb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(hb), gtk_label_new("Pages (e.g., 1-5,7):"));
    pages_e = gtk_entry_new();
    gtk_widget_set_hexpand(pages_e, TRUE);
    gtk_box_append(GTK_BOX(hb), pages_e);
    gtk_box_append(GTK_BOX(box), hb);
    
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output PDF:", &out_e, TRUE));

    GtkWidget *btn = hv_make_action_btn("Extract Pages");
    GtkWidget *status = hv_make_result_label();
    
    PdfExtCtx *ctx = g_new0(PdfExtCtx, 1);
    ctx->in_e = in_e; ctx->out_e = out_e; ctx->pages_e = pages_e; ctx->status = status;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_pdf_extract), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);

    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), status);
    return box;
}

/* ================================================================
 * PDF Delete Pages (qpdf)
 * ================================================================ */
static void on_pdf_delete(GtkButton *btn, gpointer ud) {
    (void)btn;
    PdfExtCtx *ctx = ud;
    const char *in = gtk_editable_get_text(GTK_EDITABLE(ctx->in_e));
    const char *out = gtk_editable_get_text(GTK_EDITABLE(ctx->out_e));
    const char *pages = gtk_editable_get_text(GTK_EDITABLE(ctx->pages_e));
    
    if (!in || !*in || !out || !*out || !pages || !*pages) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Fill all fields");
        return;
    }
    
    char *qi = g_shell_quote(in);
    char *qo = g_shell_quote(out);
    
    // In qpdf, to delete pages we need to specify all pages except the ones to delete.
    // Wait, it's easier to just specify pages to KEEP.
    // Actually qpdf doesn't have a direct "delete pages" flag.
    // Let's use mutool: mutool clean -N input.pdf output.pdf pages
    // No, mutool uses: mutool clean input.pdf output.pdf [pages to KEEP]
    // Wait, let's use qpdf syntax.
    // qpdf input.pdf --pages input.pdf 1-3,5-z -- output.pdf
    // "Delete" means we should ask the user for pages to KEEP, or explain it.
    // Let's label it "Pages to Keep (e.g., 1-5,8-z):".
    // Or we can just use `pdftk input.pdf cat 1-5 8-end output out.pdf`.
    char *qp = g_shell_quote(pages);
    char *cmd = g_strdup_printf("qpdf --empty --pages %s %s -- %s 2>&1", qi, qp, qo);
    g_free(qi); g_free(qo); g_free(qp);

    char *res = hv_run_cmd(cmd);
    g_free(cmd);
    if (!res || !*res || strstr(res, "WARNING"))
        gtk_label_set_text(GTK_LABEL(ctx->status), "✓ Pages deleted/kept successfully!");
    else
        gtk_label_set_text(GTK_LABEL(ctx->status), res);
    g_free(res);
}

GtkWidget *build_pdf_delete(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *in_e, *out_e, *pages_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input PDF:", &in_e, FALSE));
    
    GtkWidget *hb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(hb), gtk_label_new("Pages to KEEP (e.g., 1-5,7-z):"));
    pages_e = gtk_entry_new();
    gtk_widget_set_hexpand(pages_e, TRUE);
    gtk_box_append(GTK_BOX(hb), pages_e);
    gtk_box_append(GTK_BOX(box), hb);
    
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output PDF:", &out_e, TRUE));

    GtkWidget *btn = hv_make_action_btn("Process PDF");
    GtkWidget *status = hv_make_result_label();
    
    PdfExtCtx *ctx = g_new0(PdfExtCtx, 1);
    ctx->in_e = in_e; ctx->out_e = out_e; ctx->pages_e = pages_e; ctx->status = status;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_pdf_delete), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);

    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), status);
    return box;
}

/* ================================================================
 * PDF Rotate Pages (qpdf)
 * ================================================================ */
typedef struct { GtkWidget *in_e, *out_e, *cb_deg, *status; } PdfRotCtx;

static void on_pdf_rotate(GtkButton *btn, gpointer ud) {
    (void)btn;
    PdfRotCtx *ctx = ud;
    const char *in = gtk_editable_get_text(GTK_EDITABLE(ctx->in_e));
    const char *out = gtk_editable_get_text(GTK_EDITABLE(ctx->out_e));
    int rot = gtk_drop_down_get_selected(GTK_DROP_DOWN(ctx->cb_deg));
    
    if (!in || !*in || !out || !*out) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Fill all fields");
        return;
    }
    
    const char *degs[] = { "+90", "+180", "-90" };
    char *qi = g_shell_quote(in);
    char *qo = g_shell_quote(out);
    
    // qpdf in.pdf out.pdf --rotate=+90
    char *cmd = g_strdup_printf("qpdf %s %s --rotate=%s 2>&1", qi, qo, degs[rot]);
    g_free(qi); g_free(qo);

    char *res = hv_run_cmd(cmd);
    g_free(cmd);
    if (!res || !*res || strstr(res, "WARNING"))
        gtk_label_set_text(GTK_LABEL(ctx->status), "✓ PDF rotated successfully!");
    else
        gtk_label_set_text(GTK_LABEL(ctx->status), res);
    g_free(res);
}

GtkWidget *build_pdf_rotate(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *in_e, *out_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input PDF:", &in_e, FALSE));
    
    GtkWidget *hb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(hb), gtk_label_new("Rotation:"));
    const char *degs[] = { "90° Clockwise", "180°", "90° Counter-Clockwise", NULL };
    GtkWidget *cb_deg = gtk_drop_down_new_from_strings(degs);
    gtk_box_append(GTK_BOX(hb), cb_deg);
    gtk_box_append(GTK_BOX(box), hb);
    
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output PDF:", &out_e, TRUE));

    GtkWidget *btn = hv_make_action_btn("Rotate PDF");
    GtkWidget *status = hv_make_result_label();
    
    PdfRotCtx *ctx = g_new0(PdfRotCtx, 1);
    ctx->in_e = in_e; ctx->out_e = out_e; ctx->cb_deg = cb_deg; ctx->status = status;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_pdf_rotate), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);

    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), status);
    return box;
}

/* ================================================================
 * PDF Encrypt (qpdf)
 * ================================================================ */
typedef struct { GtkWidget *in_e, *out_e, *pass_e, *status; } PdfCryptCtx;

static void on_pdf_encrypt(GtkButton *btn, gpointer ud) {
    (void)btn;
    PdfCryptCtx *ctx = ud;
    const char *in = gtk_editable_get_text(GTK_EDITABLE(ctx->in_e));
    const char *out = gtk_editable_get_text(GTK_EDITABLE(ctx->out_e));
    const char *pass = gtk_editable_get_text(GTK_EDITABLE(ctx->pass_e));
    
    if (!in || !*in || !out || !*out || !pass || !*pass) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Fill all fields");
        return;
    }
    
    char *qi = g_shell_quote(in);
    char *qo = g_shell_quote(out);
    char *qp = g_shell_quote(pass);
    
    // qpdf --encrypt pass pass 256 -- input.pdf output.pdf
    char *cmd = g_strdup_printf("qpdf --encrypt %s %s 256 -- %s %s 2>&1", qp, qp, qi, qo);
    g_free(qi); g_free(qo); g_free(qp);

    char *res = hv_run_cmd(cmd);
    g_free(cmd);
    if (!res || !*res || strstr(res, "WARNING"))
        gtk_label_set_text(GTK_LABEL(ctx->status), "✓ PDF encrypted successfully!");
    else
        gtk_label_set_text(GTK_LABEL(ctx->status), res);
    g_free(res);
}

GtkWidget *build_pdf_encrypt(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *in_e, *out_e, *pass_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input PDF:", &in_e, FALSE));
    
    GtkWidget *hb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(hb), gtk_label_new("Password:"));
    pass_e = gtk_password_entry_new();
    gtk_widget_set_hexpand(pass_e, TRUE);
    gtk_password_entry_set_show_peek_icon(GTK_PASSWORD_ENTRY(pass_e), TRUE);
    gtk_box_append(GTK_BOX(hb), pass_e);
    gtk_box_append(GTK_BOX(box), hb);
    
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output PDF:", &out_e, TRUE));

    GtkWidget *btn = hv_make_action_btn("Encrypt PDF");
    GtkWidget *status = hv_make_result_label();
    
    PdfCryptCtx *ctx = g_new0(PdfCryptCtx, 1);
    ctx->in_e = in_e; ctx->out_e = out_e; ctx->pass_e = pass_e; ctx->status = status;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_pdf_encrypt), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);

    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), status);
    return box;
}

/* ================================================================
 * PDF Decrypt (qpdf)
 * ================================================================ */
static void on_pdf_decrypt(GtkButton *btn, gpointer ud) {
    (void)btn;
    PdfCryptCtx *ctx = ud;
    const char *in = gtk_editable_get_text(GTK_EDITABLE(ctx->in_e));
    const char *out = gtk_editable_get_text(GTK_EDITABLE(ctx->out_e));
    const char *pass = gtk_editable_get_text(GTK_EDITABLE(ctx->pass_e));
    
    if (!in || !*in || !out || !*out || !pass || !*pass) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Fill all fields");
        return;
    }
    
    char *qi = g_shell_quote(in);
    char *qo = g_shell_quote(out);
    char *qp = g_shell_quote(pass);
    
    // qpdf --password=pass --decrypt input.pdf output.pdf
    char *cmd = g_strdup_printf("qpdf --password=%s --decrypt %s %s 2>&1", qp, qi, qo);
    g_free(qi); g_free(qo); g_free(qp);

    char *res = hv_run_cmd(cmd);
    g_free(cmd);
    if (!res || !*res || strstr(res, "WARNING"))
        gtk_label_set_text(GTK_LABEL(ctx->status), "✓ PDF decrypted successfully!");
    else
        gtk_label_set_text(GTK_LABEL(ctx->status), res);
    g_free(res);
}

GtkWidget *build_pdf_decrypt(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *in_e, *out_e, *pass_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input PDF:", &in_e, FALSE));
    
    GtkWidget *hb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(hb), gtk_label_new("Password:"));
    pass_e = gtk_password_entry_new();
    gtk_widget_set_hexpand(pass_e, TRUE);
    gtk_password_entry_set_show_peek_icon(GTK_PASSWORD_ENTRY(pass_e), TRUE);
    gtk_box_append(GTK_BOX(hb), pass_e);
    gtk_box_append(GTK_BOX(box), hb);
    
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output PDF:", &out_e, TRUE));

    GtkWidget *btn = hv_make_action_btn("Decrypt PDF");
    GtkWidget *status = hv_make_result_label();
    
    PdfCryptCtx *ctx = g_new0(PdfCryptCtx, 1);
    ctx->in_e = in_e; ctx->out_e = out_e; ctx->pass_e = pass_e; ctx->status = status;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_pdf_decrypt), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);

    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), status);
    return box;
}

/* ================================================================
 * PDF Compress (ghostscript)
 * ================================================================ */
static void on_pdf_compress(GtkButton *btn, gpointer ud) {
    (void)btn;
    PdfCtx *ctx = ud;
    const char *in = gtk_editable_get_text(GTK_EDITABLE(ctx->in_e));
    const char *out = gtk_editable_get_text(GTK_EDITABLE(ctx->out_e));
    
    if (!in || !*in || !out || !*out) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Fill all fields");
        return;
    }
    
    char *qi = g_shell_quote(in);
    char *qo = g_shell_quote(out);
    
    // gs -sDEVICE=pdfwrite -dCompatibilityLevel=1.4 -dPDFSETTINGS=/screen -dNOPAUSE -dQUIET -dBATCH -sOutputFile=output.pdf input.pdf
    char *cmd = g_strdup_printf("gs -sDEVICE=pdfwrite -dCompatibilityLevel=1.4 -dPDFSETTINGS=/screen -dNOPAUSE -dQUIET -dBATCH -sOutputFile=%s %s 2>&1", qo, qi);
    g_free(qi); g_free(qo);

    char *res = hv_run_cmd(cmd);
    g_free(cmd);
    if (!res || !*res)
        gtk_label_set_text(GTK_LABEL(ctx->status), "✓ PDF compressed successfully!");
    else
        gtk_label_set_text(GTK_LABEL(ctx->status), res);
    g_free(res);
}

GtkWidget *build_pdf_compress(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *in_e, *out_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input PDF:", &in_e, FALSE));
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output PDF:", &out_e, TRUE));

    GtkWidget *btn = hv_make_action_btn("Compress PDF");
    GtkWidget *status = hv_make_result_label();
    
    PdfCtx *ctx = g_new0(PdfCtx, 1);
    ctx->in_e = in_e; ctx->out_e = out_e; ctx->status = status;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_pdf_compress), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);

    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), status);
    return box;
}

/* ================================================================
 * PDF Viewer (pdfinfo + pdftocairo)
 * ================================================================ */
typedef struct {
    char *pdf_path;
    int total_pages;
    int current_page;
    GtkWidget *pic;
    GtkWidget *lbl_page;
    GtkWidget *btn_prev;
    GtkWidget *btn_next;
} PdfViewerCtx;

static void render_current_page(PdfViewerCtx *ctx) {
    if (!ctx->pdf_path || ctx->total_pages <= 0) return;
    
    char *q_in = g_shell_quote(ctx->pdf_path);
    // pdftocairo -png -f N -l N -singlefile input.pdf output_prefix
    char *cmd = g_strdup_printf("pdftocairo -png -f %d -l %d -singlefile %s /tmp/helvetia_pdf_page", 
                                ctx->current_page, ctx->current_page, q_in);
    g_free(q_in);
    
    char *res = hv_run_cmd(cmd);
    g_free(cmd);
    g_free(res);
    
    gtk_picture_set_filename(GTK_PICTURE(ctx->pic), "/tmp/helvetia_pdf_page.png");
    
    char buf[64];
    snprintf(buf, sizeof(buf), "Page %d of %d", ctx->current_page, ctx->total_pages);
    gtk_label_set_text(GTK_LABEL(ctx->lbl_page), buf);
    
    gtk_widget_set_sensitive(ctx->btn_prev, ctx->current_page > 1);
    gtk_widget_set_sensitive(ctx->btn_next, ctx->current_page < ctx->total_pages);
}

static void on_viewer_file_set(GtkEntry *entry, gpointer ud) {
    PdfViewerCtx *ctx = ud;
    const char *path = gtk_editable_get_text(GTK_EDITABLE(entry));
    if (!path || !*path) return;
    
    g_free(ctx->pdf_path);
    ctx->pdf_path = g_strdup(path);
    
    char *q_in = g_shell_quote(path);
    char *cmd = g_strdup_printf("pdfinfo %s | grep Pages:", q_in);
    g_free(q_in);
    
    char *res = hv_run_cmd(cmd);
    g_free(cmd);
    
    if (res && strncmp(res, "Pages:", 6) == 0) {
        ctx->total_pages = atoi(res + 6);
        ctx->current_page = 1;
        render_current_page(ctx);
    } else {
        gtk_label_set_text(GTK_LABEL(ctx->lbl_page), "Failed to read PDF");
        ctx->total_pages = 0;
    }
    g_free(res);
}

static void on_viewer_prev(GtkButton *btn, gpointer ud) {
    (void)btn;
    PdfViewerCtx *ctx = ud;
    if (ctx->current_page > 1) {
        ctx->current_page--;
        render_current_page(ctx);
    }
}

static void on_viewer_next(GtkButton *btn, gpointer ud) {
    (void)btn;
    PdfViewerCtx *ctx = ud;
    if (ctx->current_page < ctx->total_pages) {
        ctx->current_page++;
        render_current_page(ctx);
    }
}

static void on_viewer_destroy(gpointer ud) {
    PdfViewerCtx *ctx = ud;
    g_free(ctx->pdf_path);
    g_free(ctx);
}

GtkWidget *build_pdf_viewer(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    
    PdfViewerCtx *ctx = g_new0(PdfViewerCtx, 1);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(on_viewer_destroy), ctx);
    
    GtkWidget *in_e;
    GtkWidget *file_row = hv_make_file_picker_row("Open PDF:", &in_e, FALSE);
    gtk_box_append(GTK_BOX(box), file_row);
    g_signal_connect(in_e, "changed", G_CALLBACK(on_viewer_file_set), ctx);
    
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(toolbar, GTK_ALIGN_CENTER);
    
    ctx->btn_prev = gtk_button_new_with_label("◀ Previous");
    ctx->btn_next = gtk_button_new_with_label("Next ▶");
    ctx->lbl_page = gtk_label_new("No PDF loaded");
    
    gtk_widget_set_sensitive(ctx->btn_prev, FALSE);
    gtk_widget_set_sensitive(ctx->btn_next, FALSE);
    
    g_signal_connect(ctx->btn_prev, "clicked", G_CALLBACK(on_viewer_prev), ctx);
    g_signal_connect(ctx->btn_next, "clicked", G_CALLBACK(on_viewer_next), ctx);
    
    gtk_box_append(GTK_BOX(toolbar), ctx->btn_prev);
    gtk_box_append(GTK_BOX(toolbar), ctx->lbl_page);
    gtk_box_append(GTK_BOX(toolbar), ctx->btn_next);
    gtk_box_append(GTK_BOX(box), toolbar);
    
    ctx->pic = gtk_picture_new();
    gtk_picture_set_can_shrink(GTK_PICTURE(ctx->pic), TRUE);
    gtk_picture_set_content_fit(GTK_PICTURE(ctx->pic), GTK_CONTENT_FIT_CONTAIN);
    
    GtkWidget *sw = gtk_scrolled_window_new();
    gtk_widget_set_hexpand(sw, TRUE);
    gtk_widget_set_vexpand(sw, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sw), ctx->pic);
    
    gtk_box_append(GTK_BOX(box), sw);
    
    return box;
}

/* ================================================================
 * PDF Thumbnails (pdftocairo)
 * ================================================================ */
typedef struct {
    char *pdf_path;
    GtkWidget *flowbox;
    GtkWidget *status;
} PdfThumbsCtx;

static void on_generate_thumbnails(GtkButton *btn, gpointer ud) {
    (void)btn;
    PdfThumbsCtx *ctx = ud;
    
    if (!ctx->pdf_path) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Please select a PDF first");
        return;
    }
    
    // Clear flowbox
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(ctx->flowbox)) != NULL) {
        gtk_flow_box_remove(GTK_FLOW_BOX(ctx->flowbox), child);
    }
    
    gtk_label_set_text(GTK_LABEL(ctx->status), "Generating thumbnails...");
    
    char *q_in = g_shell_quote(ctx->pdf_path);
    // Remove old thumbnails
    char *cmd_rm = g_strdup("rm -f /tmp/helvetia_thumb-*.jpg");
    hv_run_cmd(cmd_rm);
    g_free(cmd_rm);
    
    // pdftocairo -jpeg -scale-to 200 input.pdf /tmp/helvetia_thumb
    char *cmd = g_strdup_printf("pdftocairo -jpeg -scale-to 200 %s /tmp/helvetia_thumb", q_in);
    g_free(q_in);
    
    hv_run_cmd(cmd);
    g_free(cmd);
    
    // Now load thumbnails into flowbox
    int i = 1;
    while (1) {
        char path[256];
        snprintf(path, sizeof(path), "/tmp/helvetia_thumb-%d.jpg", i);
        if (!g_file_test(path, G_FILE_TEST_EXISTS)) break;
        
        GtkWidget *pic = gtk_picture_new_for_filename(path);
        gtk_widget_set_size_request(pic, 150, 200);
        gtk_picture_set_can_shrink(GTK_PICTURE(pic), TRUE);
        gtk_picture_set_content_fit(GTK_PICTURE(pic), GTK_CONTENT_FIT_CONTAIN);
        
        GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        gtk_box_append(GTK_BOX(vbox), pic);
        char lbl_txt[32];
        snprintf(lbl_txt, sizeof(lbl_txt), "Page %d", i);
        GtkWidget *lbl = gtk_label_new(lbl_txt);
        gtk_box_append(GTK_BOX(vbox), lbl);
        
        gtk_flow_box_insert(GTK_FLOW_BOX(ctx->flowbox), vbox, -1);
        i++;
    }
    
    if (i > 1) {
        char status_txt[64];
        snprintf(status_txt, sizeof(status_txt), "✓ Generated %d thumbnails!", i - 1);
        gtk_label_set_text(GTK_LABEL(ctx->status), status_txt);
    } else {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Failed to generate thumbnails");
    }
}

static void on_thumbs_file_set(GtkEntry *entry, gpointer ud) {
    PdfThumbsCtx *ctx = ud;
    const char *path = gtk_editable_get_text(GTK_EDITABLE(entry));
    if (!path || !*path) return;
    
    g_free(ctx->pdf_path);
    ctx->pdf_path = g_strdup(path);
}

static void on_thumbs_destroy(gpointer ud) {
    PdfThumbsCtx *ctx = ud;
    g_free(ctx->pdf_path);
    g_free(ctx);
}

GtkWidget *build_pdf_thumbnails(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    
    PdfThumbsCtx *ctx = g_new0(PdfThumbsCtx, 1);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(on_thumbs_destroy), ctx);
    
    GtkWidget *in_e;
    GtkWidget *file_row = hv_make_file_picker_row("Input PDF:", &in_e, FALSE);
    gtk_box_append(GTK_BOX(box), file_row);
    g_signal_connect(in_e, "changed", G_CALLBACK(on_thumbs_file_set), ctx);
    
    GtkWidget *btn = hv_make_action_btn("Generate Thumbnails");
    ctx->status = hv_make_result_label();
    
    g_signal_connect(btn, "clicked", G_CALLBACK(on_generate_thumbnails), ctx);
    
    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), ctx->status);
    
    ctx->flowbox = gtk_flow_box_new();
    gtk_widget_set_valign(ctx->flowbox, GTK_ALIGN_START);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(ctx->flowbox), 10);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(ctx->flowbox), GTK_SELECTION_NONE);
    
    GtkWidget *sw = gtk_scrolled_window_new();
    gtk_widget_set_hexpand(sw, TRUE);
    gtk_widget_set_vexpand(sw, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sw), ctx->flowbox);
    
    gtk_box_append(GTK_BOX(box), sw);
    
    return box;
}

/* ================================================================
 * Image to PDF (convert)
 * ================================================================ */
static void on_image_to_pdf(GtkButton *btn, gpointer ud) {
    (void)btn;
    PdfCtx *ctx = ud;
    const char *in = gtk_editable_get_text(GTK_EDITABLE(ctx->in_e));
    const char *out = gtk_editable_get_text(GTK_EDITABLE(ctx->out_e));
    if (!in || !*in || !out || !*out) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Fill both paths");
        return;
    }
    char *qi = g_shell_quote(in);
    char *qo = g_shell_quote(out);
    
    char *cmd = g_strdup_printf("convert %s %s 2>&1", qi, qo);
    g_free(qi); g_free(qo);

    char *res = hv_run_cmd(cmd);
    g_free(cmd);
    if (!res || !*res || strstr(res, "WARNING"))
        gtk_label_set_text(GTK_LABEL(ctx->status), "✓ Image converted to PDF!");
    else
        gtk_label_set_text(GTK_LABEL(ctx->status), res);
    g_free(res);
}

GtkWidget *build_image_to_pdf(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *in_e, *out_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input Image:", &in_e, FALSE));
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output PDF:", &out_e, TRUE));

    GtkWidget *btn = hv_make_action_btn("Convert to PDF");
    GtkWidget *status = hv_make_result_label();
    
    PdfCtx *ctx = g_new0(PdfCtx, 1);
    ctx->in_e = in_e; ctx->out_e = out_e; ctx->status = status;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_image_to_pdf), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);

    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), status);
    return box;
}

/* ================================================================
 * PDF to HTML (pdftohtml)
 * ================================================================ */
static void on_pdf_to_html(GtkButton *btn, gpointer ud) {
    (void)btn;
    PdfCtx *ctx = ud;
    const char *in = gtk_editable_get_text(GTK_EDITABLE(ctx->in_e));
    const char *out = gtk_editable_get_text(GTK_EDITABLE(ctx->out_e));
    if (!in || !*in || !out || !*out) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Fill both paths");
        return;
    }
    char *qi = g_shell_quote(in);
    char *qo = g_shell_quote(out);
    
    char *cmd = g_strdup_printf("pdftohtml -c -s %s %s 2>&1", qi, qo);
    g_free(qi); g_free(qo);

    char *res = hv_run_cmd(cmd);
    g_free(cmd);
    if (!res || !*res || strstr(res, "Page-"))
        gtk_label_set_text(GTK_LABEL(ctx->status), "✓ PDF converted to HTML!");
    else
        gtk_label_set_text(GTK_LABEL(ctx->status), res);
    g_free(res);
}

GtkWidget *build_pdf_to_html(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *in_e, *out_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input PDF:", &in_e, FALSE));
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output HTML:", &out_e, TRUE));

    GtkWidget *btn = hv_make_action_btn("Convert to HTML");
    GtkWidget *status = hv_make_result_label();
    
    PdfCtx *ctx = g_new0(PdfCtx, 1);
    ctx->in_e = in_e; ctx->out_e = out_e; ctx->status = status;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_pdf_to_html), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);

    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), status);
    return box;
}
