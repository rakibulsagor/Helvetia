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
