#define _POSIX_C_SOURCE 200809L
/* ================================================================
 * Helvetia — Images Module — Tool Implementations (GdkPixbuf)
 * ================================================================ */
#include "images_module.h"
#include "../../ui/widgets.h"
#include <gtk/gtk.h>
#include <glib.h>
#include <string.h>
#include <stdio.h>

/* ================================================================
 * Image Viewer + Info
 * ================================================================ */
typedef struct { GtkWidget *picture, *info_label; } ViewerCtx;

static void on_load_image(GtkButton *btn, gpointer ud) {
    (void)btn;
    ViewerCtx *ctx = ud;
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Open Image");

    /* Create a filter for image files */
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Images");
    gtk_file_filter_add_mime_type(filter, "image/*");

    /* Wrap filter in a GListModel */
    GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
    g_list_store_append(filters, filter);
    g_object_unref(filter);
    gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
    g_object_unref(filters);

    GtkRoot *root = gtk_widget_get_root(GTK_WIDGET(btn));
    GtkWindow *parent = GTK_IS_WINDOW(root) ? GTK_WINDOW(root) : NULL;

    /* We'll use the synchronous open for simplicity */
    /* But gtk_file_dialog_open is async, let's use the simple GFile path approach */
    (void)parent;
    (void)ctx;
    /* For now use a text entry to avoid async complexity */
    gtk_label_set_text(GTK_LABEL(ctx->info_label), "Use the path entry to load an image");
}

static void on_load_path(GtkButton *btn, gpointer ud) {
    (void)btn;
    ViewerCtx *ctx = ud;
    GtkWidget *entry = g_object_get_data(G_OBJECT(btn), "path-entry");
    const char *path = gtk_editable_get_text(GTK_EDITABLE(entry));
    if (!path || !*path) return;

    GError *err = NULL;
    GdkPixbuf *pb = gdk_pixbuf_new_from_file(path, &err);
    if (!pb) {
        gtk_label_set_text(GTK_LABEL(ctx->info_label),
                           err ? err->message : "Failed to load image");
        if (err) g_error_free(err);
        return;
    }

    /* Scale to fit display (max 600x400) */
    int orig_w = gdk_pixbuf_get_width(pb);
    int orig_h = gdk_pixbuf_get_height(pb);
    double scale = 1.0;
    if (orig_w > 600) scale = 600.0 / orig_w;
    if (orig_h * scale > 400) scale = 400.0 / orig_h;
    
    GdkPixbuf *scaled = gdk_pixbuf_scale_simple(pb, 
        (int)(orig_w * scale), (int)(orig_h * scale), GDK_INTERP_BILINEAR);
    g_object_unref(pb);

    gtk_picture_set_pixbuf(GTK_PICTURE(ctx->picture), scaled);
    g_object_unref(scaled);

    char info[256];
    int channels = gdk_pixbuf_get_n_channels(scaled ? scaled : pb);
    snprintf(info, sizeof info, 
             "Size: %dx%d  |  Channels: %d  |  %s",
             orig_w, orig_h, channels,
             gdk_pixbuf_get_has_alpha(scaled ? scaled : pb) ? "RGBA" : "RGB");
    gtk_label_set_text(GTK_LABEL(ctx->info_label), info);
}

GtkWidget *build_image_viewer(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    
    GtkWidget *path_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *path_entry = gtk_entry_new();
    gtk_widget_set_hexpand(path_entry, TRUE);
    gtk_entry_set_placeholder_text(GTK_ENTRY(path_entry), "/path/to/image.png");
    GtkWidget *load_btn = gtk_button_new_with_label("Load");
    gtk_widget_add_css_class(load_btn, "suggested-action");
    gtk_box_append(GTK_BOX(path_row), path_entry);
    gtk_box_append(GTK_BOX(path_row), load_btn);
    gtk_box_append(GTK_BOX(box), path_row);

    GtkWidget *picture = gtk_picture_new();
    gtk_widget_set_size_request(picture, 600, 400);
    gtk_widget_set_hexpand(picture, TRUE);
    gtk_picture_set_can_shrink(GTK_PICTURE(picture), TRUE);
    gtk_picture_set_content_fit(GTK_PICTURE(picture), GTK_CONTENT_FIT_CONTAIN);
    gtk_box_append(GTK_BOX(box), picture);

    GtkWidget *info = hv_make_result_label();
    gtk_label_set_text(GTK_LABEL(info), "Load an image to view it here");
    gtk_box_append(GTK_BOX(box), info);

    ViewerCtx *ctx = g_new0(ViewerCtx, 1);
    ctx->picture = picture;
    ctx->info_label = info;
    g_object_set_data(G_OBJECT(load_btn), "path-entry", path_entry);
    g_signal_connect(load_btn, "clicked", G_CALLBACK(on_load_path), ctx);
    g_signal_connect(path_entry, "activate", G_CALLBACK(on_load_path), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);
    
    return box;
}

/* ================================================================
 * Image Format Converter (GdkPixbuf)
 * ================================================================ */
typedef struct { GtkWidget *in_e, *out_e, *format_dd, *quality_spin, *status; } ImgConvCtx;

static void on_img_convert(GtkButton *btn, gpointer ud) {
    (void)btn;
    ImgConvCtx *ctx = ud;
    const char *in_path  = gtk_editable_get_text(GTK_EDITABLE(ctx->in_e));
    const char *out_path = gtk_editable_get_text(GTK_EDITABLE(ctx->out_e));
    
    if (!in_path || !*in_path || !out_path || !*out_path) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Fill both paths");
        return;
    }

    static const char *format_types[] = { "png", "jpeg", "bmp", "tiff", "webp" };
    guint fmt_idx = gtk_drop_down_get_selected(GTK_DROP_DOWN(ctx->format_dd));
    const char *fmt = format_types[fmt_idx < 5 ? fmt_idx : 0];
    
    GError *err = NULL;
    GdkPixbuf *pb = gdk_pixbuf_new_from_file(in_path, &err);
    if (!pb) {
        char msg[256];
        snprintf(msg, sizeof msg, "⚠ %s", err ? err->message : "Failed to load");
        gtk_label_set_text(GTK_LABEL(ctx->status), msg);
        if (err) g_error_free(err);
        return;
    }
    
    gboolean success;
    if (strcmp(fmt, "jpeg") == 0) {
        char quality_str[8];
        int quality = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->quality_spin));
        snprintf(quality_str, sizeof quality_str, "%d", quality);
        success = gdk_pixbuf_save(pb, out_path, "jpeg", &err,
                                  "quality", quality_str, NULL);
    } else {
        success = gdk_pixbuf_save(pb, out_path, fmt, &err, NULL);
    }
    g_object_unref(pb);
    
    if (success) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "✓ Image converted successfully!");
    } else {
        char msg[256];
        snprintf(msg, sizeof msg, "⚠ %s", err ? err->message : "Save failed");
        gtk_label_set_text(GTK_LABEL(ctx->status), msg);
        if (err) g_error_free(err);
    }
}

GtkWidget *build_image_converter(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *in_e, *out_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:", &in_e, FALSE));
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output image:", &out_e, TRUE));
    gtk_entry_set_placeholder_text(GTK_ENTRY(out_e), "/path/to/output.png");

    GtkWidget *fmt_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(fmt_row), gtk_label_new("Output format:"));
    const char *fmts[] = { "PNG", "JPEG", "BMP", "TIFF", "WebP", NULL };
    GtkWidget *dd = gtk_drop_down_new_from_strings(fmts);
    gtk_box_append(GTK_BOX(fmt_row), dd);
    gtk_box_append(GTK_BOX(box), fmt_row);

    GtkWidget *q_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(q_row), gtk_label_new("JPEG Quality (1-100):"));
    GtkWidget *qspin = gtk_spin_button_new_with_range(1, 100, 5);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(qspin), 85);
    gtk_box_append(GTK_BOX(q_row), qspin);
    gtk_box_append(GTK_BOX(box), q_row);

    GtkWidget *btn    = hv_make_action_btn("Convert Image");
    GtkWidget *status = hv_make_result_label();

    ImgConvCtx *ctx = g_new0(ImgConvCtx, 1);
    ctx->in_e = in_e; ctx->out_e = out_e;
    ctx->format_dd = dd; ctx->quality_spin = qspin;
    ctx->status = status;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_img_convert), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);

    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), status);
    return box;
}

/* ================================================================
 * Image Resize
 * ================================================================ */
typedef struct { GtkWidget *in_e, *out_e, *w_spin, *h_spin, *keep_ratio, *status; } ResizeCtx;

static void on_img_resize(GtkButton *btn, gpointer ud) {
    (void)btn;
    ResizeCtx *ctx = ud;
    const char *in  = gtk_editable_get_text(GTK_EDITABLE(ctx->in_e));
    const char *out = gtk_editable_get_text(GTK_EDITABLE(ctx->out_e));
    if (!in || !*in || !out || !*out) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Fill input and output paths");
        return;
    }
    int new_w = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->w_spin));
    int new_h = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->h_spin));
    gboolean keep = gtk_check_button_get_active(GTK_CHECK_BUTTON(ctx->keep_ratio));

    GError *err = NULL;
    GdkPixbuf *pb = gdk_pixbuf_new_from_file(in, &err);
    if (!pb) {
        gtk_label_set_text(GTK_LABEL(ctx->status), err ? err->message : "Load failed");
        if (err) g_error_free(err);
        return;
    }
    int orig_w = gdk_pixbuf_get_width(pb);
    int orig_h = gdk_pixbuf_get_height(pb);
    if (keep && orig_w > 0) {
        new_h = (int)((double)new_w / orig_w * orig_h);
    }
    GdkPixbuf *scaled = gdk_pixbuf_scale_simple(pb, new_w, new_h, GDK_INTERP_BILINEAR);
    g_object_unref(pb);

    gboolean ok = gdk_pixbuf_save(scaled, out, "png", &err, NULL);
    g_object_unref(scaled);
    gtk_label_set_text(GTK_LABEL(ctx->status),
                       ok ? "✓ Resized successfully!" : (err ? err->message : "Save failed"));
    if (err) g_error_free(err);
}

GtkWidget *build_image_resize(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *in_e, *out_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:", &in_e, FALSE));
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output image:", &out_e, TRUE));

    GtkWidget *dims = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(dims), gtk_label_new("Width:"));
    GtkWidget *w_spin = gtk_spin_button_new_with_range(1, 8192, 10);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(w_spin), 800);
    gtk_box_append(GTK_BOX(dims), w_spin);
    gtk_box_append(GTK_BOX(dims), gtk_label_new("Height:"));
    GtkWidget *h_spin = gtk_spin_button_new_with_range(1, 8192, 10);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(h_spin), 600);
    gtk_box_append(GTK_BOX(dims), h_spin);
    gtk_box_append(GTK_BOX(box), dims);

    GtkWidget *ratio = gtk_check_button_new_with_label("Keep aspect ratio (uses width)");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(ratio), TRUE);
    gtk_box_append(GTK_BOX(box), ratio);

    GtkWidget *btn    = hv_make_action_btn("Resize Image");
    GtkWidget *status = hv_make_result_label();

    ResizeCtx *ctx = g_new0(ResizeCtx, 1);
    ctx->in_e = in_e; ctx->out_e = out_e;
    ctx->w_spin = w_spin; ctx->h_spin = h_spin;
    ctx->keep_ratio = ratio; ctx->status = status;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_img_resize), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);

    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), status);
    return box;
}

/* ================================================================
 * Image Grayscale / Invert (pixel manipulation)
 * ================================================================ */
typedef struct { GtkWidget *in_e, *out_e, *status; int mode; } FilterCtx;

static void on_apply_filter(GtkButton *btn, gpointer ud) {
    (void)btn;
    FilterCtx *ctx = ud;
    const char *in  = gtk_editable_get_text(GTK_EDITABLE(ctx->in_e));
    const char *out = gtk_editable_get_text(GTK_EDITABLE(ctx->out_e));
    if (!in || !*in || !out || !*out) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Fill input and output paths");
        return;
    }
    GError *err = NULL;
    GdkPixbuf *src = gdk_pixbuf_new_from_file(in, &err);
    if (!src) {
        gtk_label_set_text(GTK_LABEL(ctx->status), err ? err->message : "Load failed");
        if (err) g_error_free(err);
        return;
    }
    /* Ensure RGB/RGBA */
    GdkPixbuf *pb = gdk_pixbuf_add_alpha(src, FALSE, 0, 0, 0);
    g_object_unref(src);
    if (!pb) { gtk_label_set_text(GTK_LABEL(ctx->status), "Buffer error"); return; }

    int w = gdk_pixbuf_get_width(pb);
    int h = gdk_pixbuf_get_height(pb);
    int rowstride = gdk_pixbuf_get_rowstride(pb);
    guchar *pixels = gdk_pixbuf_get_pixels(pb);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            guchar *p = pixels + y * rowstride + x * 4;
            if (ctx->mode == 0) { /* Grayscale */
                guchar gray = (guchar)(0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2]);
                p[0] = p[1] = p[2] = gray;
            } else if (ctx->mode == 1) { /* Invert */
                p[0] = 255 - p[0];
                p[1] = 255 - p[1];
                p[2] = 255 - p[2];
            }
        }
    }
    gboolean ok = gdk_pixbuf_save(pb, out, "png", &err, NULL);
    g_object_unref(pb);
    gtk_label_set_text(GTK_LABEL(ctx->status),
                       ok ? "✓ Filter applied!" : (err ? err->message : "Save failed"));
    if (err) g_error_free(err);
}

static GtkWidget *make_filter_tool(const char *label_text, int mode) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *in_e, *out_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:", &in_e, FALSE));
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output image:", &out_e, TRUE));
    GtkWidget *btn    = hv_make_action_btn(label_text);
    GtkWidget *status = hv_make_result_label();
    FilterCtx *ctx = g_new0(FilterCtx, 1);
    ctx->in_e = in_e; ctx->out_e = out_e; ctx->status = status; ctx->mode = mode;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_apply_filter), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);
    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), status);
    return box;
}

GtkWidget *build_image_grayscale(void) { return make_filter_tool("Convert to Grayscale", 0); }
GtkWidget *build_image_invert   (void) { return make_filter_tool("Invert Colors",        1); }

/* ================================================================
 * Image Crop (convert)
 * ================================================================ */
typedef struct { GtkWidget *in_e, *out_e, *geom_e, *status; } ImgCropCtx;

static void on_img_crop(GtkButton *btn, gpointer ud) {
    (void)btn;
    ImgCropCtx *ctx = ud;
    const char *in = gtk_editable_get_text(GTK_EDITABLE(ctx->in_e));
    const char *out = gtk_editable_get_text(GTK_EDITABLE(ctx->out_e));
    const char *geom = gtk_editable_get_text(GTK_EDITABLE(ctx->geom_e));
    
    if (!in || !*in || !out || !*out || !geom || !*geom) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Fill all fields");
        return;
    }
    
    char *qi = g_shell_quote(in);
    char *qo = g_shell_quote(out);
    char *qg = g_shell_quote(geom);
    
    char *cmd = g_strdup_printf("convert %s -crop %s %s 2>&1", qi, qg, qo);
    g_free(qi); g_free(qo); g_free(qg);

    char *res = hv_run_cmd(cmd);
    g_free(cmd);
    if (!res || !*res || strstr(res, "WARNING"))
        gtk_label_set_text(GTK_LABEL(ctx->status), "✓ Image cropped!");
    else
        gtk_label_set_text(GTK_LABEL(ctx->status), res);
    g_free(res);
}

GtkWidget *build_image_crop(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *in_e, *out_e, *geom_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input Image:", &in_e, FALSE));
    
    GtkWidget *hb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(hb), gtk_label_new("Geometry (WxH+X+Y):"));
    geom_e = gtk_entry_new();
    gtk_widget_set_hexpand(geom_e, TRUE);
    gtk_entry_set_placeholder_text(GTK_ENTRY(geom_e), "e.g. 800x600+10+20");
    gtk_box_append(GTK_BOX(hb), geom_e);
    gtk_box_append(GTK_BOX(box), hb);
    
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output Image:", &out_e, TRUE));

    GtkWidget *btn = hv_make_action_btn("Crop Image");
    GtkWidget *status = hv_make_result_label();
    
    ImgCropCtx *ctx = g_new0(ImgCropCtx, 1);
    ctx->in_e = in_e; ctx->out_e = out_e; ctx->geom_e = geom_e; ctx->status = status;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_img_crop), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);

    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), status);
    return box;
}

/* ================================================================
 * Image Blur (convert)
 * ================================================================ */
typedef struct { GtkWidget *in_e, *out_e, *sigma_spin, *status; } ImgBlurCtx;

static void on_img_blur(GtkButton *btn, gpointer ud) {
    (void)btn;
    ImgBlurCtx *ctx = ud;
    const char *in = gtk_editable_get_text(GTK_EDITABLE(ctx->in_e));
    const char *out = gtk_editable_get_text(GTK_EDITABLE(ctx->out_e));
    double sigma = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->sigma_spin));
    
    if (!in || !*in || !out || !*out) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Fill all fields");
        return;
    }
    
    char *qi = g_shell_quote(in);
    char *qo = g_shell_quote(out);
    
    char *cmd = g_strdup_printf("convert %s -blur 0x%.2f %s 2>&1", qi, sigma, qo);
    g_free(qi); g_free(qo);

    char *res = hv_run_cmd(cmd);
    g_free(cmd);
    if (!res || !*res || strstr(res, "WARNING"))
        gtk_label_set_text(GTK_LABEL(ctx->status), "✓ Image blurred!");
    else
        gtk_label_set_text(GTK_LABEL(ctx->status), res);
    g_free(res);
}

GtkWidget *build_image_blur(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *in_e, *out_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input Image:", &in_e, FALSE));
    
    GtkWidget *hb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(hb), gtk_label_new("Blur Amount (Sigma):"));
    GtkWidget *spin = gtk_spin_button_new_with_range(0.1, 50.0, 0.5);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), 2.0);
    gtk_box_append(GTK_BOX(hb), spin);
    gtk_box_append(GTK_BOX(box), hb);
    
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output Image:", &out_e, TRUE));

    GtkWidget *btn = hv_make_action_btn("Blur Image");
    GtkWidget *status = hv_make_result_label();
    
    ImgBlurCtx *ctx = g_new0(ImgBlurCtx, 1);
    ctx->in_e = in_e; ctx->out_e = out_e; ctx->sigma_spin = spin; ctx->status = status;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_img_blur), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);

    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), status);
    return box;
}
