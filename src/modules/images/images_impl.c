#define _POSIX_C_SOURCE 200809L
/* ================================================================
 * Helvetia — Images Module — Tool Implementations (GdkPixbuf + ImageMagick)
 * ================================================================ */
#include "images_module.h"
#include "../../ui/widgets.h"
#include <gtk/gtk.h>
#include <glib.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <gegl.h>
#include "image_processing.h"

/* ================================================================
 * Helpers shared across tools
 * ================================================================ */

/* Make a horizontal row: label + spin button, return spin */
static GtkWidget *make_spin_row(const char *label, double lo, double hi,
                                double step, double val, GtkWidget **spin_out) {
    GtkWidget *box  = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *lbl  = gtk_label_new(label);
    gtk_widget_set_size_request(lbl, 160, -1);
    gtk_widget_set_halign(lbl, GTK_ALIGN_END);
    GtkWidget *spin = gtk_spin_button_new_with_range(lo, hi, step);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), val);
    gtk_widget_set_hexpand(spin, TRUE);
    gtk_box_append(GTK_BOX(box), lbl);
    gtk_box_append(GTK_BOX(box), spin);
    if (spin_out) *spin_out = spin;
    return box;
}

/* Make a horizontal row: label + scale (slider) */
static GtkWidget *make_scale_row(const char *label, double lo, double hi,
                                 double val, GtkWidget **scale_out) {
    GtkWidget *box   = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *lbl   = gtk_label_new(label);
    gtk_widget_set_size_request(lbl, 160, -1);
    gtk_widget_set_halign(lbl, GTK_ALIGN_END);
    GtkWidget *scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, lo, hi,
                                               (hi - lo) / 100.0);
    gtk_range_set_value(GTK_RANGE(scale), val);
    gtk_scale_set_draw_value(GTK_SCALE(scale), TRUE);
    gtk_widget_set_hexpand(scale, TRUE);
    gtk_box_append(GTK_BOX(box), lbl);
    gtk_box_append(GTK_BOX(box), scale);
    if (scale_out) *scale_out = scale;
    return box;
}

/* Placeholder for interactive tools that need a full painting canvas */
static GtkWidget *make_canvas_placeholder(const char *tool_name) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_vexpand(box, TRUE);

    GtkWidget *img = gtk_image_new_from_icon_name("applications-graphics-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(img), 64);
    gtk_widget_add_css_class(img, "helvetia-fg-muted");

    char title_txt[128];
    snprintf(title_txt, sizeof title_txt, "%s", tool_name);
    GtkWidget *title = gtk_label_new(title_txt);
    gtk_widget_add_css_class(title, "helvetia-card-title");

    GtkWidget *sub = gtk_label_new(
        "This tool requires an interactive pixel canvas.\n"
        "For full editing, open your image in GIMP or Pinta.");
    gtk_widget_add_css_class(sub, "helvetia-fg-muted");
    gtk_label_set_wrap(GTK_LABEL(sub), TRUE);
    gtk_label_set_justify(GTK_LABEL(sub), GTK_JUSTIFY_CENTER);

    GtkWidget *open_btn = gtk_button_new_with_label("Open GIMP");
    gtk_widget_add_css_class(open_btn, "suggested-action");
    gtk_widget_set_halign(open_btn, GTK_ALIGN_CENTER);
    g_signal_connect_swapped(open_btn, "clicked",
        G_CALLBACK(hv_run_cmd), (gpointer)"gimp &");

    gtk_box_append(GTK_BOX(box), img);
    gtk_box_append(GTK_BOX(box), title);
    gtk_box_append(GTK_BOX(box), sub);
    gtk_box_append(GTK_BOX(box), open_btn);
    return box;
}

/* ================================================================
 * Image Viewer + Info
 * ================================================================ */
typedef struct { GtkWidget *picture, *info_label; } ViewerCtx;

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
    snprintf(info, sizeof info,
             "Size: %dx%d  |  Channels: %d  |  %s",
             orig_w, orig_h,
             gdk_pixbuf_get_n_channels(scaled),
             gdk_pixbuf_get_has_alpha(scaled) ? "RGBA" : "RGB");
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
 * Image Slideshow
 * ================================================================ */
typedef struct {
    GPtrArray   *paths;   /* owned strings */
    gint         current;
    gboolean     playing;
    guint        timer_id;
    GtkWidget   *picture;
    GtkWidget   *counter;
    GtkWidget   *play_btn;
    GtkWidget   *delay_spin;
} SlideshowCtx;

static void slideshow_load(SlideshowCtx *ctx) {
    if (!ctx->paths || ctx->paths->len == 0) return;
    if (ctx->current < 0) ctx->current = 0;
    if ((guint)ctx->current >= ctx->paths->len)
        ctx->current = 0;

    const char *path = ctx->paths->pdata[ctx->current];
    GError *err = NULL;
    GdkPixbuf *pb = gdk_pixbuf_new_from_file_at_scale(path, 800, 600, TRUE, &err);
    if (pb) {
        gtk_picture_set_pixbuf(GTK_PICTURE(ctx->picture), pb);
        g_object_unref(pb);
    }
    char txt[64];
    snprintf(txt, sizeof txt, "%d / %u", ctx->current + 1, ctx->paths->len);
    gtk_label_set_text(GTK_LABEL(ctx->counter), txt);
}

static gboolean slideshow_advance(gpointer ud) {
    SlideshowCtx *ctx = ud;
    if (!ctx->playing) return G_SOURCE_REMOVE;
    ctx->current++;
    if ((guint)ctx->current >= ctx->paths->len) ctx->current = 0;
    slideshow_load(ctx);
    return G_SOURCE_CONTINUE;
}

static void slideshow_stop(SlideshowCtx *ctx) {
    if (ctx->timer_id) {
        g_source_remove(ctx->timer_id);
        ctx->timer_id = 0;
    }
    ctx->playing = FALSE;
}

static void on_slideshow_play(GtkButton *btn, gpointer ud) {
    (void)btn;
    SlideshowCtx *ctx = ud;
    if (!ctx->paths || ctx->paths->len == 0) return;
    if (ctx->playing) {
        slideshow_stop(ctx);
        gtk_button_set_label(GTK_BUTTON(ctx->play_btn), "▶ Play");
    } else {
        ctx->playing = TRUE;
        int delay_ms = (int)(gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->delay_spin)) * 1000);
        ctx->timer_id = g_timeout_add(delay_ms, slideshow_advance, ctx);
        gtk_button_set_label(GTK_BUTTON(ctx->play_btn), "⏸ Pause");
    }
}

static void on_slideshow_prev(GtkButton *btn, gpointer ud) {
    (void)btn;
    SlideshowCtx *ctx = ud;
    slideshow_stop(ctx);
    gtk_button_set_label(GTK_BUTTON(ctx->play_btn), "▶ Play");
    ctx->current--;
    slideshow_load(ctx);
}

static void on_slideshow_next(GtkButton *btn, gpointer ud) {
    (void)btn;
    SlideshowCtx *ctx = ud;
    slideshow_stop(ctx);
    gtk_button_set_label(GTK_BUTTON(ctx->play_btn), "▶ Play");
    ctx->current++;
    slideshow_load(ctx);
}

static void on_slideshow_load_folder(GtkButton *btn, gpointer ud) {
    (void)btn;
    SlideshowCtx *ctx = ud;
    GtkWidget *entry = g_object_get_data(G_OBJECT(btn), "folder-entry");
    const char *folder = gtk_editable_get_text(GTK_EDITABLE(entry));
    if (!folder || !*folder) return;

    slideshow_stop(ctx);
    gtk_button_set_label(GTK_BUTTON(ctx->play_btn), "▶ Play");
    if (ctx->paths) g_ptr_array_free(ctx->paths, TRUE);
    ctx->paths = g_ptr_array_new_with_free_func(g_free);

    GDir *dir = g_dir_open(folder, 0, NULL);
    if (!dir) return;
    const char *name;
    static const char *exts[] = { ".png", ".jpg", ".jpeg", ".bmp",
                                   ".tiff", ".tif", ".webp", ".gif", NULL };
    while ((name = g_dir_read_name(dir))) {
        const char *ext = strrchr(name, '.');
        if (!ext) continue;
        for (int i = 0; exts[i]; i++) {
            if (g_ascii_strcasecmp(ext, exts[i]) == 0) {
                g_ptr_array_add(ctx->paths, g_build_filename(folder, name, NULL));
                break;
            }
        }
    }
    g_dir_close(dir);

    /* Sort alphabetically */
    g_ptr_array_sort(ctx->paths, (GCompareFunc)g_strcmp0);
    ctx->current = 0;
    slideshow_load(ctx);
}

static void on_slideshow_destroy(GtkWidget *w, gpointer ud) {
    (void)w;
    SlideshowCtx *ctx = ud;
    slideshow_stop(ctx);
    if (ctx->paths) g_ptr_array_free(ctx->paths, TRUE);
    g_free(ctx);
}

GtkWidget *build_image_slideshow(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    /* Folder picker row */
    GtkWidget *folder_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *folder_entry = gtk_entry_new();
    gtk_widget_set_hexpand(folder_entry, TRUE);
    gtk_entry_set_placeholder_text(GTK_ENTRY(folder_entry), "/path/to/folder");
    GtkWidget *load_btn = gtk_button_new_with_label("Load Folder");
    gtk_widget_add_css_class(load_btn, "suggested-action");
    gtk_box_append(GTK_BOX(folder_row), gtk_label_new("Folder:"));
    gtk_box_append(GTK_BOX(folder_row), folder_entry);
    gtk_box_append(GTK_BOX(folder_row), load_btn);
    gtk_box_append(GTK_BOX(box), folder_row);

    /* Picture */
    GtkWidget *picture = gtk_picture_new();
    gtk_widget_set_size_request(picture, 800, 500);
    gtk_widget_set_hexpand(picture, TRUE);
    gtk_picture_set_can_shrink(GTK_PICTURE(picture), TRUE);
    gtk_picture_set_content_fit(GTK_PICTURE(picture), GTK_CONTENT_FIT_CONTAIN);
    gtk_box_append(GTK_BOX(box), picture);

    /* Controls row */
    GtkWidget *ctrl = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(ctrl, GTK_ALIGN_CENTER);
    GtkWidget *prev_btn  = gtk_button_new_with_label("◀ Prev");
    GtkWidget *play_btn  = gtk_button_new_with_label("▶ Play");
    gtk_widget_add_css_class(play_btn, "suggested-action");
    GtkWidget *next_btn  = gtk_button_new_with_label("Next ▶");
    GtkWidget *counter   = gtk_label_new("0 / 0");
    GtkWidget *delay_lbl = gtk_label_new("Delay (s):");
    GtkWidget *delay_sp  = gtk_spin_button_new_with_range(0.5, 30.0, 0.5);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(delay_sp), 3.0);
    gtk_box_append(GTK_BOX(ctrl), prev_btn);
    gtk_box_append(GTK_BOX(ctrl), play_btn);
    gtk_box_append(GTK_BOX(ctrl), next_btn);
    gtk_box_append(GTK_BOX(ctrl), counter);
    gtk_box_append(GTK_BOX(ctrl), delay_lbl);
    gtk_box_append(GTK_BOX(ctrl), delay_sp);
    gtk_box_append(GTK_BOX(box), ctrl);

    SlideshowCtx *ctx = g_new0(SlideshowCtx, 1);
    ctx->picture    = picture;
    ctx->counter    = counter;
    ctx->play_btn   = play_btn;
    ctx->delay_spin = delay_sp;
    ctx->current    = 0;

    g_object_set_data(G_OBJECT(load_btn), "folder-entry", folder_entry);
    g_signal_connect(load_btn, "clicked", G_CALLBACK(on_slideshow_load_folder), ctx);
    g_signal_connect(prev_btn, "clicked", G_CALLBACK(on_slideshow_prev),  ctx);
    g_signal_connect(next_btn, "clicked", G_CALLBACK(on_slideshow_next),  ctx);
    g_signal_connect(play_btn, "clicked", G_CALLBACK(on_slideshow_play),  ctx);
    g_signal_connect(box, "destroy", G_CALLBACK(on_slideshow_destroy),    ctx);

    return box;
}

/* ================================================================
 * Image Thumbnail Grid
 * ================================================================ */
typedef struct { GtkWidget *win; const char *path; } ThumbData;

static void on_thumb_clicked(GtkGestureClick *g, int n, double x, double y, gpointer ud) {
    (void)g; (void)n; (void)x; (void)y;
    ThumbData *td = ud;
    /* Open in default viewer */
    char *cmd = g_strdup_printf("xdg-open %s &", td->path);
    hv_run_cmd(cmd);
    g_free(cmd);
}

static void on_thumb_destroy(GtkWidget *w, gpointer ud) {
    (void)w;
    ThumbData *td = ud;
    g_free((char *)td->path);
    g_free(td);
}

static void on_thumbgrid_load(GtkButton *btn, gpointer ud) {
    (void)btn;
    GtkWidget *flow = g_object_get_data(G_OBJECT(btn), "flowbox");
    GtkWidget *entry = g_object_get_data(G_OBJECT(btn), "folder-entry");
    const char *folder = gtk_editable_get_text(GTK_EDITABLE(entry));
    if (!folder || !*folder) return;

    /* Clear existing */
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(flow)) != NULL)
        gtk_flow_box_remove(GTK_FLOW_BOX(flow), child);

    GDir *dir = g_dir_open(folder, 0, NULL);
    if (!dir) return;
    static const char *exts[] = { ".png", ".jpg", ".jpeg", ".bmp",
                                   ".tiff", ".tif", ".webp", ".gif", NULL };
    const char *name;
    while ((name = g_dir_read_name(dir))) {
        const char *ext = strrchr(name, '.');
        if (!ext) continue;
        gboolean ok = FALSE;
        for (int i = 0; exts[i]; i++)
            if (g_ascii_strcasecmp(ext, exts[i]) == 0) { ok = TRUE; break; }
        if (!ok) continue;

        char *full = g_build_filename(folder, name, NULL);
        GError *err = NULL;
        GdkPixbuf *pb = gdk_pixbuf_new_from_file_at_scale(full, 120, 90, TRUE, &err);
        if (!pb) { g_free(full); if (err) g_error_free(err); continue; }

        GtkWidget *img = gtk_picture_new();
        gtk_picture_set_pixbuf(GTK_PICTURE(img), pb);
        gtk_widget_set_size_request(img, 120, 90);
        gtk_picture_set_content_fit(GTK_PICTURE(img), GTK_CONTENT_FIT_CONTAIN);
        g_object_unref(pb);

        GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        gtk_box_append(GTK_BOX(card), img);
        GtkWidget *lbl = gtk_label_new(name);
        gtk_label_set_ellipsize(GTK_LABEL(lbl), PANGO_ELLIPSIZE_END);
        gtk_widget_set_size_request(lbl, 120, -1);
        gtk_box_append(GTK_BOX(card), lbl);

        ThumbData *td = g_new(ThumbData, 1);
        td->path = full;
        GtkGesture *click = gtk_gesture_click_new();
        g_signal_connect(click, "pressed", G_CALLBACK(on_thumb_clicked), td);
        g_signal_connect(card, "destroy", G_CALLBACK(on_thumb_destroy), td);
        gtk_widget_add_controller(card, GTK_EVENT_CONTROLLER(click));
        gtk_widget_set_cursor_from_name(card, "pointer");

        gtk_flow_box_insert(GTK_FLOW_BOX(flow), card, -1);
    }
    g_dir_close(dir);
}

GtkWidget *build_image_thumbnail_grid(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *entry = gtk_entry_new();
    gtk_widget_set_hexpand(entry, TRUE);
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "/path/to/folder");
    GtkWidget *btn = gtk_button_new_with_label("Load");
    gtk_widget_add_css_class(btn, "suggested-action");
    gtk_box_append(GTK_BOX(row), gtk_label_new("Folder:"));
    gtk_box_append(GTK_BOX(row), entry);
    gtk_box_append(GTK_BOX(row), btn);
    gtk_box_append(GTK_BOX(box), row);

    GtkWidget *sw = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(sw, TRUE);
    gtk_widget_set_size_request(sw, -1, 400);

    GtkWidget *flow = gtk_flow_box_new();
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(flow), 2);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flow), 8);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flow), GTK_SELECTION_NONE);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(flow), 8);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(flow), 8);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sw), flow);
    gtk_box_append(GTK_BOX(box), sw);

    g_object_set_data(G_OBJECT(btn), "flowbox", flow);
    g_object_set_data(G_OBJECT(btn), "folder-entry", entry);
    g_signal_connect(btn, "clicked", G_CALLBACK(on_thumbgrid_load), NULL);

    return box;
}

/* ================================================================
 * EXIF / Metadata Viewer (via identify -verbose)
 * ================================================================ */
static void on_exif_load(GtkButton *btn, gpointer ud) {
    (void)btn;
    GtkWidget *tv = ud;
    GtkWidget *entry = g_object_get_data(G_OBJECT(btn), "path-entry");
    const char *path = gtk_editable_get_text(GTK_EDITABLE(entry));
    if (!path || !*path) return;
    char *qp = g_shell_quote(path);
    char *cmd = g_strdup_printf("identify -verbose %s 2>&1", qp);
    g_free(qp);
    char *res = hv_run_cmd(cmd);
    g_free(cmd);
    hv_textview_set_text(tv, res ? res : "No output");
    g_free(res);
}

static GtkWidget *make_exif_tool(const char *placeholder) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *in_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row(placeholder, &in_e, FALSE));
    GtkWidget *btn = hv_make_action_btn("Load Info");
    GtkWidget *tv_sw, *tv;
    tv_sw = hv_make_text_view(&tv, FALSE);
    gtk_widget_set_vexpand(tv_sw, TRUE);
    gtk_widget_set_size_request(tv_sw, -1, 300);
    g_object_set_data(G_OBJECT(btn), "path-entry", in_e);
    g_signal_connect(btn, "clicked", G_CALLBACK(on_exif_load), tv);
    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), tv_sw);
    return box;
}

GtkWidget *build_exif_viewer(void)        { return make_exif_tool("Image file:"); }
GtkWidget *build_metadata_inspector(void) { return make_exif_tool("Image file:"); }

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
            } else if (ctx->mode == 2) { /* Sepia */
                int r = (int)(p[0] * 0.393 + p[1] * 0.769 + p[2] * 0.189);
                int g = (int)(p[0] * 0.349 + p[1] * 0.686 + p[2] * 0.168);
                int b = (int)(p[0] * 0.272 + p[1] * 0.534 + p[2] * 0.131);
                p[0] = (guchar)MIN(255, r);
                p[1] = (guchar)MIN(255, g);
                p[2] = (guchar)MIN(255, b);
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
GtkWidget *build_image_sepia    (void) { return make_filter_tool("Apply Sepia Tone",     2); }

/* ================================================================
 * Threshold (pixel manipulation)
 * ================================================================ */
typedef struct { GtkWidget *in_e, *out_e, *thresh_spin, *status; } ThreshCtx;

static void on_threshold(GtkButton *btn, gpointer ud) {
    (void)btn;
    ThreshCtx *ctx = ud;
    const char *in  = gtk_editable_get_text(GTK_EDITABLE(ctx->in_e));
    const char *out = gtk_editable_get_text(GTK_EDITABLE(ctx->out_e));
    if (!in || !*in || !out || !*out) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Fill paths"); return;
    }
    int thresh = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->thresh_spin));
    GError *err = NULL;
    GdkPixbuf *src = gdk_pixbuf_new_from_file(in, &err);
    if (!src) {
        gtk_label_set_text(GTK_LABEL(ctx->status), err ? err->message : "Load failed");
        if (err) g_error_free(err); return;
    }
    GdkPixbuf *pb = gdk_pixbuf_add_alpha(src, FALSE, 0, 0, 0);
    g_object_unref(src);
    int w = gdk_pixbuf_get_width(pb), h = gdk_pixbuf_get_height(pb);
    int rowstride = gdk_pixbuf_get_rowstride(pb);
    guchar *pixels = gdk_pixbuf_get_pixels(pb);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            guchar *p = pixels + y * rowstride + x * 4;
            guchar lum = (guchar)(0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2]);
            guchar val = lum >= thresh ? 255 : 0;
            p[0] = p[1] = p[2] = val;
        }
    }
    gboolean ok = gdk_pixbuf_save(pb, out, "png", &err, NULL);
    g_object_unref(pb);
    gtk_label_set_text(GTK_LABEL(ctx->status),
                       ok ? "✓ Threshold applied!" : (err ? err->message : "Save failed"));
    if (err) g_error_free(err);
}

GtkWidget *build_image_threshold(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *in_e, *out_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:", &in_e, FALSE));
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output image:", &out_e, TRUE));
    GtkWidget *spin;
    gtk_box_append(GTK_BOX(box), make_spin_row("Threshold (0-255):", 0, 255, 1, 128, &spin));
    GtkWidget *btn = hv_make_action_btn("Apply Threshold");
    GtkWidget *status = hv_make_result_label();
    ThreshCtx *ctx = g_new0(ThreshCtx, 1);
    ctx->in_e = in_e; ctx->out_e = out_e; ctx->thresh_spin = spin; ctx->status = status;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_threshold), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);
    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), status);
    return box;
}

/* ================================================================
 * Film Grain (pixel noise)
 * ================================================================ */
typedef struct { GtkWidget *in_e, *out_e, *amount_spin, *status; } GrainCtx;

static void on_film_grain(GtkButton *btn, gpointer ud) {
    (void)btn;
    GrainCtx *ctx = ud;
    const char *in  = gtk_editable_get_text(GTK_EDITABLE(ctx->in_e));
    const char *out = gtk_editable_get_text(GTK_EDITABLE(ctx->out_e));
    if (!in || !*in || !out || !*out) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Fill paths"); return;
    }
    int amount = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->amount_spin));
    GError *err = NULL;
    GdkPixbuf *src = gdk_pixbuf_new_from_file(in, &err);
    if (!src) {
        gtk_label_set_text(GTK_LABEL(ctx->status), err ? err->message : "Load failed");
        if (err) g_error_free(err); return;
    }
    GdkPixbuf *pb = gdk_pixbuf_add_alpha(src, FALSE, 0, 0, 0);
    g_object_unref(src);
    int w = gdk_pixbuf_get_width(pb), h = gdk_pixbuf_get_height(pb);
    int rowstride = gdk_pixbuf_get_rowstride(pb);
    guchar *pixels = gdk_pixbuf_get_pixels(pb);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            guchar *p = pixels + y * rowstride + x * 4;
            int noise = (g_random_int_range(-amount, amount + 1));
            p[0] = (guchar)CLAMP((int)p[0] + noise, 0, 255);
            p[1] = (guchar)CLAMP((int)p[1] + noise, 0, 255);
            p[2] = (guchar)CLAMP((int)p[2] + noise, 0, 255);
        }
    }
    gboolean ok = gdk_pixbuf_save(pb, out, "png", &err, NULL);
    g_object_unref(pb);
    gtk_label_set_text(GTK_LABEL(ctx->status),
                       ok ? "✓ Film grain applied!" : (err ? err->message : "Save failed"));
    if (err) g_error_free(err);
}

GtkWidget *build_image_film_grain(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *in_e, *out_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:", &in_e, FALSE));
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output image:", &out_e, TRUE));
    GtkWidget *spin;
    gtk_box_append(GTK_BOX(box), make_spin_row("Grain Amount (1-80):", 1, 80, 1, 15, &spin));
    GtkWidget *btn = hv_make_action_btn("Add Film Grain");
    GtkWidget *status = hv_make_result_label();
    GrainCtx *ctx = g_new0(GrainCtx, 1);
    ctx->in_e = in_e; ctx->out_e = out_e; ctx->amount_spin = spin; ctx->status = status;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_film_grain), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);
    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), status);
    return box;
}

/* ================================================================
 * Pixelate (in-process)
 * ================================================================ */
typedef struct { GtkWidget *in_e, *out_e, *size_spin, *status; } PixelateCtx;

static void on_pixelate(GtkButton *btn, gpointer ud) {
    (void)btn;
    PixelateCtx *ctx = ud;
    const char *in  = gtk_editable_get_text(GTK_EDITABLE(ctx->in_e));
    const char *out = gtk_editable_get_text(GTK_EDITABLE(ctx->out_e));
    if (!in || !*in || !out || !*out) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Fill paths"); return;
    }
    int psize = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->size_spin));
    GError *err = NULL;
    GdkPixbuf *src = gdk_pixbuf_new_from_file(in, &err);
    if (!src) {
        gtk_label_set_text(GTK_LABEL(ctx->status), err ? err->message : "Load failed");
        if (err) g_error_free(err); return;
    }
    int w = gdk_pixbuf_get_width(src), h = gdk_pixbuf_get_height(src);
    /* Downscale then upscale for pixelation */
    GdkPixbuf *small = gdk_pixbuf_scale_simple(src,
        MAX(1, w / psize), MAX(1, h / psize), GDK_INTERP_NEAREST);
    g_object_unref(src);
    GdkPixbuf *pb = gdk_pixbuf_scale_simple(small, w, h, GDK_INTERP_NEAREST);
    g_object_unref(small);
    gboolean ok = gdk_pixbuf_save(pb, out, "png", &err, NULL);
    g_object_unref(pb);
    gtk_label_set_text(GTK_LABEL(ctx->status),
                       ok ? "✓ Pixelated!" : (err ? err->message : "Save failed"));
    if (err) g_error_free(err);
}

GtkWidget *build_image_pixelate(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *in_e, *out_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:", &in_e, FALSE));
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output image:", &out_e, TRUE));
    GtkWidget *spin;
    gtk_box_append(GTK_BOX(box), make_spin_row("Pixel Block Size:", 2, 64, 1, 8, &spin));
    GtkWidget *btn = hv_make_action_btn("Pixelate");
    GtkWidget *status = hv_make_result_label();
    PixelateCtx *ctx = g_new0(PixelateCtx, 1);
    ctx->in_e = in_e; ctx->out_e = out_e; ctx->size_spin = spin; ctx->status = status;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_pixelate), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);
    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), status);
    return box;
}

/* ================================================================
 * Eyedropper — pick pixel color at X,Y
 * ================================================================ */
typedef struct { GtkWidget *in_e, *x_spin, *y_spin, *result; } EyedropCtx;

static void on_eyedrop(GtkButton *btn, gpointer ud) {
    (void)btn;
    EyedropCtx *ctx = ud;
    const char *path = gtk_editable_get_text(GTK_EDITABLE(ctx->in_e));
    if (!path || !*path) return;
    int px = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->x_spin));
    int py = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->y_spin));
    GError *err = NULL;
    GdkPixbuf *pb = gdk_pixbuf_new_from_file(path, &err);
    if (!pb) {
        gtk_label_set_text(GTK_LABEL(ctx->result), err ? err->message : "Load failed");
        if (err) g_error_free(err); return;
    }
    int w = gdk_pixbuf_get_width(pb), h = gdk_pixbuf_get_height(pb);
    if (px < 0 || px >= w || py < 0 || py >= h) {
        gtk_label_set_text(GTK_LABEL(ctx->result), "⚠ Coordinates out of bounds");
        g_object_unref(pb); return;
    }
    int nchannels = gdk_pixbuf_get_n_channels(pb);
    int rowstride  = gdk_pixbuf_get_rowstride(pb);
    guchar *pixels = gdk_pixbuf_get_pixels(pb);
    guchar *p = pixels + py * rowstride + px * nchannels;
    char txt[128];
    if (nchannels >= 4)
        snprintf(txt, sizeof txt, "R:%d G:%d B:%d A:%d  |  #%02X%02X%02X",
                 p[0], p[1], p[2], p[3], p[0], p[1], p[2]);
    else
        snprintf(txt, sizeof txt, "R:%d G:%d B:%d  |  #%02X%02X%02X",
                 p[0], p[1], p[2], p[0], p[1], p[2]);
    gtk_label_set_text(GTK_LABEL(ctx->result), txt);
    g_object_unref(pb);
}

GtkWidget *build_image_eyedropper(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *in_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Image file:", &in_e, FALSE));
    GtkWidget *x_spin, *y_spin;
    gtk_box_append(GTK_BOX(box), make_spin_row("X (pixel column):", 0, 65535, 1, 0, &x_spin));
    gtk_box_append(GTK_BOX(box), make_spin_row("Y (pixel row):",    0, 65535, 1, 0, &y_spin));
    GtkWidget *btn = hv_make_action_btn("Pick Color");
    GtkWidget *result = hv_make_result_label();
    EyedropCtx *ctx = g_new0(EyedropCtx, 1);
    ctx->in_e = in_e; ctx->x_spin = x_spin; ctx->y_spin = y_spin; ctx->result = result;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_eyedrop), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);
    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), result);
    return box;
}

/* ================================================================
 * Image to Base64
 * ================================================================ */
static void on_img_to_base64(GtkButton *btn, gpointer ud) {
    (void)btn;
    GtkWidget *tv = ud;
    GtkWidget *entry = g_object_get_data(G_OBJECT(btn), "path-entry");
    const char *path = gtk_editable_get_text(GTK_EDITABLE(entry));
    if (!path || !*path) return;
    gchar *data;
    gsize  len;
    if (!g_file_get_contents(path, &data, &len, NULL)) {
        hv_textview_set_text(tv, "Failed to read file");
        return;
    }
    gchar *b64 = g_base64_encode((guchar *)data, len);
    g_free(data);
    /* Prepend data URI */
    const char *mime = "image/png";
    const char *ext  = strrchr(path, '.');
    if (ext) {
        if (g_ascii_strcasecmp(ext, ".jpg") == 0 || g_ascii_strcasecmp(ext, ".jpeg") == 0)
            mime = "image/jpeg";
        else if (g_ascii_strcasecmp(ext, ".webp") == 0)
            mime = "image/webp";
        else if (g_ascii_strcasecmp(ext, ".gif") == 0)
            mime = "image/gif";
    }
    char *out = g_strdup_printf("data:%s;base64,%s", mime, b64);
    g_free(b64);
    hv_textview_set_text(tv, out);
    g_free(out);
}

GtkWidget *build_image_to_base64(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *in_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Image file:", &in_e, FALSE));
    GtkWidget *btn = hv_make_action_btn("Convert to Base64");
    GtkWidget *tv_sw, *tv;
    tv_sw = hv_make_text_view(&tv, FALSE);
    gtk_widget_set_vexpand(tv_sw, TRUE);
    g_object_set_data(G_OBJECT(btn), "path-entry", in_e);
    g_signal_connect(btn, "clicked", G_CALLBACK(on_img_to_base64), tv);
    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), tv_sw);
    return box;
}

/* ================================================================
 * Generic ImageMagick command tool
 * (used by all IM-based tools)
 * ================================================================ */
typedef struct ImCmdCtx_ {
    GtkWidget *in_e, *out_e, *status;
    /* up to 4 parameter widgets (spin / scale / entry / dropdown) */
    GtkWidget *p[4];
    /* callback that builds the IM args string */
    char *(*build_args)(struct ImCmdCtx_ *ctx);
    /* button label for build */
    const char *output_ext;
} ImCmdCtx;

/* ------------------------------------------------------------------ */
/* Rotate */
static char *args_rotate(ImCmdCtx *ctx) {
    double angle = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[0]));
    return g_strdup_printf("-rotate %.1f", angle);
}

/* Flip */
static char *args_flip_h(ImCmdCtx *ctx) { (void)ctx; return g_strdup("-flop"); }
static char *args_flip_v(ImCmdCtx *ctx) { (void)ctx; return g_strdup("-flip"); }

/* Straighten */
static char *args_straighten(ImCmdCtx *ctx) {
    double angle = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[0]));
    return g_strdup_printf("-rotate %.2f -gravity Center", angle);
}

/* Perspective Correct */
static char *args_perspective(ImCmdCtx *ctx) {
    /* 4 source -> 4 dest points entered as comma-separated string */
    const char *coords = gtk_editable_get_text(GTK_EDITABLE(ctx->p[0]));
    return g_strdup_printf("-distort Perspective '%s'", coords);
}

/* Canvas Resize */
static char *args_canvas_resize(ImCmdCtx *ctx) {
    int w = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[0]));
    int h = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[1]));
    guint grav = gtk_drop_down_get_selected(GTK_DROP_DOWN(ctx->p[2]));
    static const char *gravities[] = {
        "NorthWest","North","NorthEast","West","Center","East","SouthWest","South","SouthEast"
    };
    const char *g = gravities[grav < 9 ? grav : 4];
    return g_strdup_printf("-gravity %s -extent %dx%d", g, w, h);
}

/* Auto-crop */
static char *args_autocrop(ImCmdCtx *ctx) {
    double fuzz = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[0]));
    return g_strdup_printf("-fuzz %.0f%% -trim +repage", fuzz);
}

/* Scale */
static char *args_scale(ImCmdCtx *ctx) {
    int w = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[0]));
    int h = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[1]));
    return g_strdup_printf("-scale %dx%d", w, h);
}

/* Skew */
static char *args_skew(ImCmdCtx *ctx) {
    double sx = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[0]));
    double sy = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[1]));
    return g_strdup_printf("-shear %.1fx%.1f", sx, sy);
}

/* Distort */
static char *args_distort(ImCmdCtx *ctx) {
    double deg = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[0]));
    guint type = gtk_drop_down_get_selected(GTK_DROP_DOWN(ctx->p[1]));
    static const char *dtypes[] = { "Arc", "Barrel", "Polar", "ScaleRotateTranslate" };
    const char *dt = dtypes[type < 4 ? type : 0];
    return g_strdup_printf("-distort %s '%.1f'", dt, deg);
}

/* Warp */
static char *args_warp(ImCmdCtx *ctx) {
    double swirl = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[0]));
    double wave_a = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[1]));
    double wave_l = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[2]));
    return g_strdup_printf("-swirl %.0f -wave %.0fx%.0f", swirl, wave_a, wave_l);
}

/* Brightness/Contrast */
static char *args_brightness_contrast(ImCmdCtx *ctx) {
    double b = gtk_range_get_value(GTK_RANGE(ctx->p[0]));
    double c = gtk_range_get_value(GTK_RANGE(ctx->p[1]));
    return g_strdup_printf("-brightness-contrast %.0fx%.0f", b, c);
}

/* Levels */
static char *args_levels(ImCmdCtx *ctx) {
    double black = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[0]));
    double white = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[1]));
    double gamma = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[2]));
    return g_strdup_printf("-level '%.0f%%,%.0f%%,%.2f'", black, white, gamma);
}

/* Exposure */
static char *args_exposure(ImCmdCtx *ctx) {
    double ev = gtk_range_get_value(GTK_RANGE(ctx->p[0]));
    double mult = pow(2.0, ev);
    return g_strdup_printf("-evaluate Multiply %.4f", mult);
}

/* Saturation / Vibrance (modulate: B,S,H where defaults are 100,100,100) */
static char *args_saturation(ImCmdCtx *ctx) {
    double sat = gtk_range_get_value(GTK_RANGE(ctx->p[0]));
    /* sat slider -100..+100 → modulate 0..200 */
    double mod_sat = 100.0 + sat;
    return g_strdup_printf("-modulate 100,%.0f,100", mod_sat);
}

/* Hue Shift */
static char *args_hue(ImCmdCtx *ctx) {
    double hue = gtk_range_get_value(GTK_RANGE(ctx->p[0]));
    /* IM modulate hue: 0-200, 100=no change, step 1 = 1.8° */
    double mod_hue = 100.0 + hue / 1.8;
    return g_strdup_printf("-modulate 100,100,%.0f", mod_hue);
}

/* White Balance — temperature slider: negative=cooler, positive=warmer */
static char *args_white_balance(ImCmdCtx *ctx) {
    double temp = gtk_range_get_value(GTK_RANGE(ctx->p[0]));
    /* adjust R and B channels via IM -evaluate */
    double r_factor = 1.0 + temp / 200.0;
    double b_factor = 1.0 - temp / 200.0;
    r_factor = CLAMP(r_factor, 0.5, 2.0);
    b_factor = CLAMP(b_factor, 0.5, 2.0);
    return g_strdup_printf("-channel R -evaluate Multiply %.3f "
                           "-channel B -evaluate Multiply %.3f +channel",
                           r_factor, b_factor);
}

/* Color Balance */
static char *args_color_balance(ImCmdCtx *ctx) {
    double r = gtk_range_get_value(GTK_RANGE(ctx->p[0]));
    double g = gtk_range_get_value(GTK_RANGE(ctx->p[1]));
    double b = gtk_range_get_value(GTK_RANGE(ctx->p[2]));
    return g_strdup_printf("-channel R -evaluate Add %.0f "
                           "-channel G -evaluate Add %.0f "
                           "-channel B -evaluate Add %.0f +channel",
                           r, g, b);
}

/* Shadows / Highlights */
static char *args_shadows_highlights(ImCmdCtx *ctx) {
    double shadow = gtk_range_get_value(GTK_RANGE(ctx->p[0]));
    double hilight = gtk_range_get_value(GTK_RANGE(ctx->p[1]));
    return g_strdup_printf("-level '%.0f%%,%.0f%%'", shadow, 100.0 - hilight);
}

/* Gamma */
static char *args_gamma(ImCmdCtx *ctx) {
    double g = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[0]));
    return g_strdup_printf("-gamma %.2f", g);
}

/* Auto Enhance */
static char *args_auto_enhance(ImCmdCtx *ctx) {
    (void)ctx;
    return g_strdup("-auto-level -auto-gamma");
}

/* Sharpen */
static char *args_sharpen(ImCmdCtx *ctx) {
    double r = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[0]));
    double s = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[1]));
    return g_strdup_printf("-sharpen %.1fx%.1f", r, s);
}

/* Unsharp Mask */
static char *args_unsharp(ImCmdCtx *ctx) {
    double r = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[0]));
    double s = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[1]));
    double a = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[2]));
    double t = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[3]));
    return g_strdup_printf("-unsharp %.1fx%.1f+%.2f+%.2f", r, s, a, t);
}

/* Noise Reduction */
static char *args_noise_reduce(ImCmdCtx *ctx) {
    int passes = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[0]));
    GString *s = g_string_new(NULL);
    for (int i = 0; i < passes; i++) g_string_append(s, "-despeckle ");
    return g_string_free(s, FALSE);
}

/* Denoise */
static char *args_denoise(ImCmdCtx *ctx) {
    (void)ctx;
    return g_strdup("-enhance -enhance");
}

/* Posterize */
static char *args_posterize(ImCmdCtx *ctx) {
    int levels = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[0]));
    return g_strdup_printf("-posterize %d", levels);
}

/* Vignette */
static char *args_vignette(ImCmdCtx *ctx) {
    double sigma = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[0]));
    double x     = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[1]));
    double y     = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[2]));
    return g_strdup_printf("-vignette %.0fx%.0f+%.0f+%.0f", sigma * 2, sigma, x, y);
}

/* Glow */
static char *args_glow(ImCmdCtx *ctx) {
    double sigma = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[0]));
    return g_strdup_printf("\\( +clone -blur 0x%.1f \\) -compose Screen -composite", sigma);
}

/* Emboss */
static char *args_emboss(ImCmdCtx *ctx) {
    double sigma = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[0]));
    return g_strdup_printf("-emboss %.1f", sigma);
}

/* Edge Detect */
static char *args_edge(ImCmdCtx *ctx) {
    double r = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[0]));
    return g_strdup_printf("-edge %.1f", r);
}

/* Mosaic */
static char *args_mosaic(ImCmdCtx *ctx) {
    int pct = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[0]));
    return g_strdup_printf("-scale %d%% -scale 10000%%", pct);
}

/* Background Removal */
static char *args_bg_remove(ImCmdCtx *ctx) {
    const char *color = gtk_editable_get_text(GTK_EDITABLE(ctx->p[0]));
    double fuzz = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[1]));
    if (!color || !*color) color = "white";
    return g_strdup_printf("-fuzz %.0f%% -transparent '%s'", fuzz, color);
}

/* Portrait Retouch */
static char *args_portrait(ImCmdCtx *ctx) {
    (void)ctx;
    return g_strdup("-unsharp 0x1+0.5+0 -modulate 100,110,100 -auto-level");
}

/* Blemish Removal (paint/spot-heal) */
static char *args_blemish(ImCmdCtx *ctx) {
    int x = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[0]));
    int y = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[1]));
    int r = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[2]));
    return g_strdup_printf("-region %dx%d+%d+%d -paint %d +region",
                           r * 2, r * 2, x - r, y - r, r);
}

/* Skin Smoothing */
static char *args_skin_smooth(ImCmdCtx *ctx) {
    double sigma = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[0]));
    return g_strdup_printf("-blur 0x%.1f -unsharp 0x1+0.3+0", sigma);
}

/* Watermark */
static char *args_watermark_text(ImCmdCtx *ctx) {
    const char *text = gtk_editable_get_text(GTK_EDITABLE(ctx->p[0]));
    double alpha = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[1]));
    guint grav = gtk_drop_down_get_selected(GTK_DROP_DOWN(ctx->p[2]));
    static const char *gravities[] = { "SouthEast","South","Center","NorthEast","North" };
    const char *g = gravities[grav < 5 ? grav : 0];
    return g_strdup_printf("-gravity %s -fill \"rgba(255,255,255,%.2f)\" "
                           "-pointsize 36 -annotate 0 '%s'",
                           g, alpha / 100.0, text);
}

/* Border / Frame */
static char *args_border(ImCmdCtx *ctx) {
    int w = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[0]));
    const char *color = gtk_editable_get_text(GTK_EDITABLE(ctx->p[1]));
    if (!color || !*color) color = "black";
    return g_strdup_printf("-bordercolor '%s' -border %d", color, w);
}

/* Drop Shadow */
static char *args_drop_shadow(ImCmdCtx *ctx) {
    double sigma = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[0]));
    int xoff = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[1]));
    int yoff = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[2]));
    return g_strdup_printf("\\( +clone -background black -shadow 80x%.0f+%d+%d \\)"
                           " +swap -background none -layers merge",
                           sigma, xoff, yoff);
}

/* Pixel Art Scaler */
static char *args_pixel_art(ImCmdCtx *ctx) {
    int scale = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[0]));
    return g_strdup_printf("-filter point -resize %d%%", scale);
}

/* Image Compression */
static char *args_compress(ImCmdCtx *ctx) {
    int q = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[0]));
    return g_strdup_printf("-quality %d", q);
}

/* Lossless PNG optimizer */
static char *args_lossless(ImCmdCtx *ctx) {
    (void)ctx;
    return g_strdup("-define png:compression-level=9 "
                    "-define png:compression-strategy=1 -strip");
}

/* SVG to PNG */
static char *args_svg_to_png(ImCmdCtx *ctx) {
    int dpi = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[0]));
    return g_strdup_printf("-density %d -background none", dpi);
}

/* Opacity Control */
static char *args_opacity(ImCmdCtx *ctx) {
    double pct = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p[0]));
    return g_strdup_printf("-alpha set -channel Alpha -evaluate Multiply %.3f +channel",
                           pct / 100.0);
}

/* Screenshot */
static char *args_screenshot(ImCmdCtx *ctx) {
    (void)ctx;
    return NULL; /* handled specially */
}

/* ------------------------------------------------------------------ */
/* Generic executor */
static void on_imcmd_run(GtkButton *btn, gpointer ud) {
    (void)btn;
    ImCmdCtx *ctx = ud;
    const char *in  = gtk_editable_get_text(GTK_EDITABLE(ctx->in_e));
    const char *out = ctx->out_e
                    ? gtk_editable_get_text(GTK_EDITABLE(ctx->out_e))
                    : NULL;

    char *args = ctx->build_args(ctx);

    char *cmd;
    if (ctx->out_e && out && *out) {
        char *qi = g_shell_quote(in);
        char *qo = g_shell_quote(out);
        cmd = g_strdup_printf("convert %s %s %s 2>&1", qi, args ? args : "", qo);
        g_free(qi); g_free(qo);
    } else if (in && *in) {
        char *qi = g_shell_quote(in);
        cmd = g_strdup_printf("convert %s %s /tmp/hv_preview_out.png 2>&1", qi, args ? args : "");
        g_free(qi);
    } else {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Fill the input path");
        g_free(args);
        return;
    }
    g_free(args);

    char *res = hv_run_cmd(cmd);
    g_free(cmd);

    if (!res || !*res || strstr(res, "WARNING") == res)
        gtk_label_set_text(GTK_LABEL(ctx->status), "✓ Done!");
    else
        gtk_label_set_text(GTK_LABEL(ctx->status), res);
    g_free(res);
}

/* ------------------------------------------------------------------ */
/* Flip tool has two buttons, special handling */
typedef struct { GtkWidget *in_e, *out_e, *status; } FlipCtx;

static void on_flip(GtkButton *btn, gpointer ud) {
    FlipCtx *ctx = ud;
    const char *flag = g_object_get_data(G_OBJECT(btn), "flip-flag");
    const char *in  = gtk_editable_get_text(GTK_EDITABLE(ctx->in_e));
    const char *out = gtk_editable_get_text(GTK_EDITABLE(ctx->out_e));
    if (!in || !*in || !out || !*out) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Fill both paths"); return;
    }
    char *qi = g_shell_quote(in);
    char *qo = g_shell_quote(out);
    char *cmd = g_strdup_printf("convert %s %s %s 2>&1", qi, flag, qo);
    g_free(qi); g_free(qo);
    char *res = hv_run_cmd(cmd);
    g_free(cmd);
    if (!res || !*res || strstr(res, "WARNING") == res)
        gtk_label_set_text(GTK_LABEL(ctx->status), "✓ Done!");
    else
        gtk_label_set_text(GTK_LABEL(ctx->status), res);
    g_free(res);
}

GtkWidget *build_image_flip(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *in_e, *out_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:", &in_e, FALSE));
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output image:", &out_e, TRUE));
    GtkWidget *btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *h_btn = hv_make_action_btn("Flip Horizontal");
    GtkWidget *v_btn = gtk_button_new_with_label("Flip Vertical");
    gtk_box_append(GTK_BOX(btn_row), h_btn);
    gtk_box_append(GTK_BOX(btn_row), v_btn);
    gtk_box_append(GTK_BOX(box), btn_row);
    GtkWidget *status = hv_make_result_label();
    FlipCtx *ctx = g_new0(FlipCtx, 1);
    ctx->in_e = in_e; ctx->out_e = out_e; ctx->status = status;
    g_object_set_data(G_OBJECT(h_btn), "flip-flag", "-flop");
    g_object_set_data(G_OBJECT(v_btn), "flip-flag", "-flip");
    g_signal_connect(h_btn, "clicked", G_CALLBACK(on_flip), ctx);
    g_signal_connect(v_btn, "clicked", G_CALLBACK(on_flip), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);
    gtk_box_append(GTK_BOX(box), status);
    return box;
}

/* ------------------------------------------------------------------ */
/* Free Transform */
typedef struct { GtkWidget *in_e, *out_e, *rotate_sp, *scale_sp, *shear_sp, *status; } FreeXfCtx;
static void on_free_xf(GtkButton *btn, gpointer ud) {
    (void)btn;
    FreeXfCtx *ctx = ud;
    const char *in  = gtk_editable_get_text(GTK_EDITABLE(ctx->in_e));
    const char *out = gtk_editable_get_text(GTK_EDITABLE(ctx->out_e));
    if (!in || !*in || !out || !*out) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Fill both paths"); return;
    }
    double r = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->rotate_sp));
    double s = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->scale_sp));
    double sh= gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->shear_sp));
    char *qi = g_shell_quote(in);
    char *qo = g_shell_quote(out);
    char *cmd = g_strdup_printf(
        "convert %s -rotate %.1f -scale %.0f%% -shear %.1f %s 2>&1",
        qi, r, s, sh, qo);
    g_free(qi); g_free(qo);
    char *res = hv_run_cmd(cmd);
    g_free(cmd);
    if (!res || !*res || strstr(res, "WARNING") == res)
        gtk_label_set_text(GTK_LABEL(ctx->status), "✓ Done!");
    else gtk_label_set_text(GTK_LABEL(ctx->status), res);
    g_free(res);
}

GtkWidget *build_image_free_transform(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *in_e, *out_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:", &in_e, FALSE));
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output image:", &out_e, TRUE));
    GtkWidget *r_sp, *s_sp, *sh_sp;
    gtk_box_append(GTK_BOX(box), make_spin_row("Rotate (°):", -360, 360, 0.5, 0, &r_sp));
    gtk_box_append(GTK_BOX(box), make_spin_row("Scale (%):", 1, 1000, 5, 100, &s_sp));
    gtk_box_append(GTK_BOX(box), make_spin_row("Shear X (°):", -89, 89, 1, 0, &sh_sp));
    GtkWidget *btn = hv_make_action_btn("Apply Transform");
    GtkWidget *status = hv_make_result_label();
    FreeXfCtx *ctx = g_new0(FreeXfCtx, 1);
    ctx->in_e=in_e; ctx->out_e=out_e;
    ctx->rotate_sp=r_sp; ctx->scale_sp=s_sp; ctx->shear_sp=sh_sp; ctx->status=status;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_free_xf), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);
    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), status);
    return box;
}

/* ------------------------------------------------------------------ */
/* Blend Modes — composite two images */
typedef struct { GtkWidget *in1_e, *in2_e, *out_e, *mode_dd, *status; } BlendCtx;
static void on_blend(GtkButton *btn, gpointer ud) {
    (void)btn;
    BlendCtx *ctx = ud;
    const char *in1 = gtk_editable_get_text(GTK_EDITABLE(ctx->in1_e));
    const char *in2 = gtk_editable_get_text(GTK_EDITABLE(ctx->in2_e));
    const char *out = gtk_editable_get_text(GTK_EDITABLE(ctx->out_e));
    if (!in1||!*in1||!in2||!*in2||!out||!*out) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Fill all paths"); return;
    }
    static const char *modes[] = {
        "Multiply","Screen","Overlay","HardLight","SoftLight",
        "Difference","Exclusion","Dodge","Burn","Darken","Lighten"
    };
    guint mi = gtk_drop_down_get_selected(GTK_DROP_DOWN(ctx->mode_dd));
    const char *mode = modes[mi < 11 ? mi : 0];
    char *q1=g_shell_quote(in1), *q2=g_shell_quote(in2), *qo=g_shell_quote(out);
    char *cmd = g_strdup_printf(
        "convert %s %s -compose %s -composite %s 2>&1", q1, q2, mode, qo);
    g_free(q1); g_free(q2); g_free(qo);
    char *res = hv_run_cmd(cmd); g_free(cmd);
    gtk_label_set_text(GTK_LABEL(ctx->status),
                       (!res||!*res||strstr(res,"WARNING")==res) ? "✓ Done!" : res);
    g_free(res);
}

GtkWidget *build_image_blend_modes(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *in1_e, *in2_e, *out_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Base image:", &in1_e, FALSE));
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Blend image:", &in2_e, FALSE));
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output:", &out_e, TRUE));
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(row), gtk_label_new("Blend mode:"));
    const char *modes[] = {"Multiply","Screen","Overlay","HardLight","SoftLight",
                           "Difference","Exclusion","Dodge","Burn","Darken","Lighten",NULL};
    GtkWidget *dd = gtk_drop_down_new_from_strings(modes);
    gtk_box_append(GTK_BOX(row), dd);
    gtk_box_append(GTK_BOX(box), row);
    GtkWidget *btn = hv_make_action_btn("Apply Blend");
    GtkWidget *status = hv_make_result_label();
    BlendCtx *ctx = g_new0(BlendCtx, 1);
    ctx->in1_e=in1_e; ctx->in2_e=in2_e; ctx->out_e=out_e;
    ctx->mode_dd=dd; ctx->status=status;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_blend), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);
    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), status);
    return box;
}

/* ------------------------------------------------------------------ */
/* Background Replace */
typedef struct { GtkWidget *in_e, *bg_e, *out_e, *color_e, *fuzz_sp, *status; } BgReplCtx;
static void on_bg_replace(GtkButton *btn, gpointer ud) {
    (void)btn;
    BgReplCtx *ctx = ud;
    const char *in  = gtk_editable_get_text(GTK_EDITABLE(ctx->in_e));
    const char *bg  = gtk_editable_get_text(GTK_EDITABLE(ctx->bg_e));
    const char *out = gtk_editable_get_text(GTK_EDITABLE(ctx->out_e));
    const char *col = gtk_editable_get_text(GTK_EDITABLE(ctx->color_e));
    double fuzz = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->fuzz_sp));
    if (!in||!*in||!bg||!*bg||!out||!*out) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Fill all paths"); return;
    }
    if (!col || !*col) col = "white";
    char *qi=g_shell_quote(in), *qb=g_shell_quote(bg), *qo=g_shell_quote(out);
    char *cmd = g_strdup_printf(
        "convert %s -fuzz %.0f%% -transparent '%s' %s "
        "+swap -background none -layers merge %s 2>&1",
        qi, fuzz, col, qb, qo);
    g_free(qi); g_free(qb); g_free(qo);
    char *res = hv_run_cmd(cmd); g_free(cmd);
    gtk_label_set_text(GTK_LABEL(ctx->status),
        (!res||!*res||strstr(res,"WARNING")==res) ? "✓ Done!" : res);
    g_free(res);
}

GtkWidget *build_image_background_replace(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *in_e, *bg_e, *out_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Subject image:", &in_e, FALSE));
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("New background:", &bg_e, FALSE));
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output:", &out_e, TRUE));
    GtkWidget *color_e;
    gtk_box_append(GTK_BOX(box), hv_make_entry_row("BG color to remove:", &color_e));
    gtk_entry_set_placeholder_text(GTK_ENTRY(color_e), "white");
    GtkWidget *fuzz_sp;
    gtk_box_append(GTK_BOX(box), make_spin_row("Fuzz %:", 1, 50, 1, 10, &fuzz_sp));
    GtkWidget *btn = hv_make_action_btn("Replace Background");
    GtkWidget *status = hv_make_result_label();
    BgReplCtx *ctx = g_new0(BgReplCtx, 1);
    ctx->in_e=in_e; ctx->bg_e=bg_e; ctx->out_e=out_e;
    ctx->color_e=color_e; ctx->fuzz_sp=fuzz_sp; ctx->status=status;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_bg_replace), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);
    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), status);
    return box;
}

/* ------------------------------------------------------------------ */
/* Red-eye Removal */
typedef struct { GtkWidget *in_e, *out_e, *x_sp, *y_sp, *r_sp, *status; } RedeyeCtx;
static void on_redeye(GtkButton *btn, gpointer ud) {
    (void)btn;
    RedeyeCtx *ctx = ud;
    const char *in  = gtk_editable_get_text(GTK_EDITABLE(ctx->in_e));
    const char *out = gtk_editable_get_text(GTK_EDITABLE(ctx->out_e));
    if (!in||!*in||!out||!*out) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Fill paths"); return;
    }
    int x = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->x_sp));
    int y = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->y_sp));
    int r = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->r_sp));
    int d = r * 2;
    char *qi=g_shell_quote(in), *qo=g_shell_quote(out);
    /* In the eye region: reduce red channel, fill with gray average */
    char *cmd = g_strdup_printf(
        "convert %s -region %dx%d+%d+%d "
        "-channel R -evaluate Multiply 0.3 +channel "
        "+region %s 2>&1",
        qi, d, d, x-r, y-r, qo);
    g_free(qi); g_free(qo);
    char *res = hv_run_cmd(cmd); g_free(cmd);
    gtk_label_set_text(GTK_LABEL(ctx->status),
        (!res||!*res||strstr(res,"WARNING")==res) ? "✓ Red-eye reduced!" : res);
    g_free(res);
}

GtkWidget *build_image_red_eye_removal(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *in_e, *out_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:", &in_e, FALSE));
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output image:", &out_e, TRUE));
    GtkWidget *note = gtk_label_new("Specify the center of each eye and the radius:");
    gtk_widget_set_halign(note, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box), note);
    GtkWidget *x_sp, *y_sp, *r_sp;
    gtk_box_append(GTK_BOX(box), make_spin_row("Eye center X (px):", 0, 65535, 1, 100, &x_sp));
    gtk_box_append(GTK_BOX(box), make_spin_row("Eye center Y (px):", 0, 65535, 1, 100, &y_sp));
    gtk_box_append(GTK_BOX(box), make_spin_row("Eye radius (px):",   1, 200,   1, 20,  &r_sp));
    GtkWidget *btn = hv_make_action_btn("Remove Red Eye");
    GtkWidget *status = hv_make_result_label();
    RedeyeCtx *ctx = g_new0(RedeyeCtx, 1);
    ctx->in_e=in_e; ctx->out_e=out_e;
    ctx->x_sp=x_sp; ctx->y_sp=y_sp; ctx->r_sp=r_sp; ctx->status=status;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_redeye), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);
    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), status);
    return box;
}

/* ------------------------------------------------------------------ */
/* Collage Maker */
typedef struct {
    GtkWidget *entries[6];
    GtkWidget *layout_dd, *gap_sp, *out_e, *status;
} CollageCtx;

static void on_collage(GtkButton *btn, gpointer ud) {
    (void)btn;
    CollageCtx *ctx = ud;
    const char *out = gtk_editable_get_text(GTK_EDITABLE(ctx->out_e));
    if (!out||!*out) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Set output path"); return;
    }
    guint layout = gtk_drop_down_get_selected(GTK_DROP_DOWN(ctx->layout_dd));
    int gap = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->gap_sp));

    GString *inputs = g_string_new(NULL);
    int count = 0;
    for (int i = 0; i < 6; i++) {
        const char *p = gtk_editable_get_text(GTK_EDITABLE(ctx->entries[i]));
        if (p && *p) {
            char *q = g_shell_quote(p);
            g_string_append_printf(inputs, "%s ", q);
            g_free(q);
            count++;
        }
    }
    if (count < 2) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Add at least 2 images");
        g_string_free(inputs, TRUE); return;
    }
    const char *append_flag = (layout == 0) ? "+append" : "-append";
    char *qo = g_shell_quote(out);
    char *cmd;
    if (layout == 2) {
        /* Grid: use montage */
        int cols = (int)ceil(sqrt((double)count));
        cmd = g_strdup_printf("montage %s-geometry +%d+%d -tile %dx %s 2>&1",
                              inputs->str, gap, gap, cols, qo);
    } else {
        cmd = g_strdup_printf("convert %s %s %s 2>&1",
                              inputs->str, append_flag, qo);
    }
    g_string_free(inputs, TRUE); g_free(qo);
    char *res = hv_run_cmd(cmd); g_free(cmd);
    gtk_label_set_text(GTK_LABEL(ctx->status),
        (!res||!*res||strstr(res,"WARNING")==res) ? "✓ Collage created!" : res);
    g_free(res);
}

GtkWidget *build_image_collage_maker(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_box_append(GTK_BOX(box), gtk_label_new("Add up to 6 images:"));
    CollageCtx *ctx = g_new0(CollageCtx, 1);
    for (int i = 0; i < 6; i++) {
        GtkWidget *e;
        char lbl[32]; snprintf(lbl, sizeof lbl, "Image %d:", i+1);
        gtk_box_append(GTK_BOX(box), hv_make_file_picker_row(lbl, &e, FALSE));
        ctx->entries[i] = e;
    }
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(row), gtk_label_new("Layout:"));
    const char *layouts[] = {"Horizontal","Vertical","Grid",NULL};
    GtkWidget *dd = gtk_drop_down_new_from_strings(layouts);
    gtk_box_append(GTK_BOX(row), dd);
    gtk_box_append(GTK_BOX(row), gtk_label_new("Gap:"));
    GtkWidget *gap_sp = gtk_spin_button_new_with_range(0, 100, 2);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(gap_sp), 4);
    gtk_box_append(GTK_BOX(row), gap_sp);
    gtk_box_append(GTK_BOX(box), row);
    GtkWidget *out_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output:", &out_e, TRUE));
    GtkWidget *btn = hv_make_action_btn("Create Collage");
    GtkWidget *status = hv_make_result_label();
    ctx->layout_dd=dd; ctx->gap_sp=gap_sp; ctx->out_e=out_e; ctx->status=status;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_collage), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);
    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), status);
    return box;
}

/* ------------------------------------------------------------------ */
/* Batch Convert */
typedef struct { GtkWidget *folder_e, *fmt_dd, *quality_sp, *out_folder_e, *status, *tv; } BatchCtx;
static void on_batch_convert(GtkButton *btn, gpointer ud) {
    (void)btn;
    BatchCtx *ctx = ud;
    const char *folder = gtk_editable_get_text(GTK_EDITABLE(ctx->folder_e));
    const char *out_f  = gtk_editable_get_text(GTK_EDITABLE(ctx->out_folder_e));
    if (!folder||!*folder||!out_f||!*out_f) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Fill both folders"); return;
    }
    static const char *exts[] = { "png","jpg","bmp","tiff","webp",NULL };
    guint fi = gtk_drop_down_get_selected(GTK_DROP_DOWN(ctx->fmt_dd));
    const char *out_ext = exts[fi < 5 ? fi : 0];
    int quality = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->quality_sp));

    GDir *dir = g_dir_open(folder, 0, NULL);
    if (!dir) { gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Cannot open folder"); return; }

    static const char *img_exts[] = { ".png",".jpg",".jpeg",".bmp",
                                      ".tiff",".tif",".webp",".gif",NULL };
    GString *log = g_string_new(NULL);
    const char *name;
    int done = 0, failed = 0;
    while ((name = g_dir_read_name(dir))) {
        const char *e = strrchr(name, '.');
        if (!e) continue;
        gboolean ok = FALSE;
        for (int i = 0; img_exts[i]; i++)
            if (g_ascii_strcasecmp(e, img_exts[i]) == 0) { ok = TRUE; break; }
        if (!ok) continue;

        char *in_path  = g_build_filename(folder, name, NULL);
        /* Build output filename */
        char *base = g_strdup(name);
        char *dot  = strrchr(base, '.');
        if (dot) *dot = '\0';
        char *out_name = g_strdup_printf("%s.%s", base, out_ext);
        char *out_path = g_build_filename(out_f, out_name, NULL);
        g_free(base); g_free(out_name);

        char *qi = g_shell_quote(in_path);
        char *qo = g_shell_quote(out_path);
        char *cmd = g_strdup_printf(
            "convert %s -quality %d %s 2>&1", qi, quality, qo);
        g_free(qi); g_free(qo); g_free(in_path); g_free(out_path);

        char *res = hv_run_cmd(cmd); g_free(cmd);
        if (!res || !*res)
            { done++; g_string_append_printf(log, "✓ %s\n", name); }
        else
            { failed++; g_string_append_printf(log, "✗ %s: %s\n", name, res); }
        g_free(res);
    }
    g_dir_close(dir);

    hv_textview_set_text(ctx->tv, log->str);
    char summary[64];
    snprintf(summary, sizeof summary, "Done: %d  Failed: %d", done, failed);
    gtk_label_set_text(GTK_LABEL(ctx->status), summary);
    g_string_free(log, TRUE);
}

GtkWidget *build_image_batch_convert(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *folder_e, *out_folder_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input folder:", &folder_e, FALSE));
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output folder:", &out_folder_e, FALSE));
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(row), gtk_label_new("Output format:"));
    const char *fmts[] = {"PNG","JPEG","BMP","TIFF","WebP",NULL};
    GtkWidget *dd = gtk_drop_down_new_from_strings(fmts);
    gtk_box_append(GTK_BOX(row), dd);
    gtk_box_append(GTK_BOX(row), gtk_label_new("Quality:"));
    GtkWidget *qs = gtk_spin_button_new_with_range(1,100,5);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(qs), 85);
    gtk_box_append(GTK_BOX(row), qs);
    gtk_box_append(GTK_BOX(box), row);
    GtkWidget *btn = hv_make_action_btn("Batch Convert");
    GtkWidget *status = hv_make_result_label();
    GtkWidget *tv_sw, *tv;
    tv_sw = hv_make_text_view(&tv, FALSE);
    gtk_widget_set_vexpand(tv_sw, TRUE);
    BatchCtx *ctx = g_new0(BatchCtx, 1);
    ctx->folder_e=folder_e; ctx->fmt_dd=dd; ctx->quality_sp=qs;
    ctx->out_folder_e=out_folder_e; ctx->status=status; ctx->tv=tv;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_batch_convert), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);
    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), status);
    gtk_box_append(GTK_BOX(box), tv_sw);
    return box;
}

/* ------------------------------------------------------------------ */
/* QR / Barcode Reader */
static void on_qr_read(GtkButton *btn, gpointer ud) {
    (void)btn;
    GtkWidget *tv = ud;
    GtkWidget *entry = g_object_get_data(G_OBJECT(btn), "path-entry");
    const char *path = gtk_editable_get_text(GTK_EDITABLE(entry));
    if (!path || !*path) return;
    char *qp = g_shell_quote(path);
    /* Try zbarimg first, fall back to identify */
    char *cmd = g_strdup_printf(
        "zbarimg --quiet --raw %s 2>&1 || identify -verbose %s 2>&1 | grep -i 'comment\\|QR\\|bar' || echo 'zbarimg not installed; install it for QR/barcode reading'",
        qp, qp);
    g_free(qp);
    char *res = hv_run_cmd(cmd); g_free(cmd);
    hv_textview_set_text(tv, res ? res : "No result");
    g_free(res);
}

static GtkWidget *make_barcode_tool(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *in_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Image file:", &in_e, FALSE));
    GtkWidget *btn = hv_make_action_btn("Read Code");
    GtkWidget *tv_sw, *tv;
    tv_sw = hv_make_text_view(&tv, FALSE);
    gtk_widget_set_vexpand(tv_sw, TRUE);
    g_object_set_data(G_OBJECT(btn), "path-entry", in_e);
    g_signal_connect(btn, "clicked", G_CALLBACK(on_qr_read), tv);
    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), tv_sw);
    return box;
}

GtkWidget *build_image_qr_reader(void)      { return make_barcode_tool(); }
GtkWidget *build_image_barcode_reader(void) { return make_barcode_tool(); }

/* ------------------------------------------------------------------ */
/* Steganography — hide/reveal text in LSBs */
typedef struct { GtkWidget *in_e, *out_e, *msg_tv, *status; } StegCtx;

static void on_steg_hide(GtkButton *btn, gpointer ud) {
    (void)btn;
    StegCtx *ctx = ud;
    const char *in  = gtk_editable_get_text(GTK_EDITABLE(ctx->in_e));
    const char *out = gtk_editable_get_text(GTK_EDITABLE(ctx->out_e));
    if (!in||!*in||!out||!*out) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Fill paths"); return;
    }
    char *msg = hv_textview_get_text(ctx->msg_tv);
    if (!msg || !*msg) { gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Enter message"); g_free(msg); return; }

    GError *err = NULL;
    GdkPixbuf *src = gdk_pixbuf_new_from_file(in, &err);
    if (!src) {
        gtk_label_set_text(GTK_LABEL(ctx->status), err ? err->message : "Load failed");
        if (err) g_error_free(err); g_free(msg); return;
    }
    GdkPixbuf *pb = gdk_pixbuf_add_alpha(src, FALSE, 0, 0, 0);
    g_object_unref(src);

    int w = gdk_pixbuf_get_width(pb), h = gdk_pixbuf_get_height(pb);
    int rowstride = gdk_pixbuf_get_rowstride(pb);
    guchar *pixels = gdk_pixbuf_get_pixels(pb);
    gsize msg_len = strlen(msg);
    /* Encode length (4 bytes) then message in LSBs of R channel */
    gsize capacity = (gsize)(w * h);
    if (msg_len + 4 > capacity) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Message too long for this image");
        g_object_unref(pb); g_free(msg); return;
    }
    /* Write 4-byte length header */
    guchar header[4] = {
        (guchar)(msg_len >> 24), (guchar)(msg_len >> 16),
        (guchar)(msg_len >>  8), (guchar)(msg_len)
    };
    int bit_idx = 0;
    for (int b = 0; b < 4; b++) {
        for (int bit = 7; bit >= 0; bit--) {
            int py = bit_idx / w, px = bit_idx % w;
            guchar *p = pixels + py * rowstride + px * 4;
            p[0] = (p[0] & 0xFE) | ((header[b] >> bit) & 1);
            bit_idx++;
        }
    }
    for (gsize i = 0; i < msg_len; i++) {
        for (int bit = 7; bit >= 0; bit--) {
            int py = bit_idx / w, px = bit_idx % w;
            guchar *p = pixels + py * rowstride + px * 4;
            p[0] = (p[0] & 0xFE) | (((guchar)msg[i] >> bit) & 1);
            bit_idx++;
        }
    }
    gboolean ok = gdk_pixbuf_save(pb, out, "png", &err, NULL);
    g_object_unref(pb); g_free(msg);
    gtk_label_set_text(GTK_LABEL(ctx->status),
                       ok ? "✓ Message hidden!" : (err ? err->message : "Save failed"));
    if (err) g_error_free(err);
}

static void on_steg_reveal(GtkButton *btn, gpointer ud) {
    (void)btn;
    StegCtx *ctx = ud;
    const char *in = gtk_editable_get_text(GTK_EDITABLE(ctx->in_e));
    if (!in||!*in) { gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Fill input path"); return; }
    GError *err = NULL;
    GdkPixbuf *src = gdk_pixbuf_new_from_file(in, &err);
    if (!src) {
        gtk_label_set_text(GTK_LABEL(ctx->status), err ? err->message : "Load failed");
        if (err) g_error_free(err); return;
    }
    GdkPixbuf *pb = gdk_pixbuf_add_alpha(src, FALSE, 0, 0, 0);
    g_object_unref(src);
    int w = gdk_pixbuf_get_width(pb), h = gdk_pixbuf_get_height(pb);
    int rowstride = gdk_pixbuf_get_rowstride(pb);
    guchar *pixels = gdk_pixbuf_get_pixels(pb);
    /* Read 4-byte length */
    int bit_idx = 0;
    guchar header[4] = {0};
    for (int b = 0; b < 4; b++) {
        for (int bit = 7; bit >= 0; bit--) {
            int py = bit_idx / w, px = bit_idx % w;
            guchar *p = pixels + py * rowstride + px * 4;
            header[b] = (guchar)((header[b] << 1) | (p[0] & 1));
            bit_idx++;
        }
    }
    gsize msg_len = ((gsize)header[0] << 24) | ((gsize)header[1] << 16) |
                    ((gsize)header[2] << 8)  |  (gsize)header[3];
    if (msg_len == 0 || msg_len > (gsize)(w * h - 32)) {
        hv_textview_set_text(ctx->msg_tv, "(No hidden message found)");
        g_object_unref(pb); return;
    }
    char *msg = g_malloc(msg_len + 1);
    for (gsize i = 0; i < msg_len; i++) {
        msg[i] = 0;
        for (int bit = 7; bit >= 0; bit--) {
            int py = bit_idx / w, px = bit_idx % w;
            guchar *p = pixels + py * rowstride + px * 4;
            msg[i] = (char)((msg[i] << 1) | (p[0] & 1));
            bit_idx++;
        }
    }
    msg[msg_len] = '\0';
    hv_textview_set_text(ctx->msg_tv, msg);
    g_free(msg);
    g_object_unref(pb);
    gtk_label_set_text(GTK_LABEL(ctx->status), "✓ Message extracted!");
}

GtkWidget *build_image_steganography(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *in_e, *out_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Carrier image:", &in_e, FALSE));
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output PNG:", &out_e, TRUE));
    gtk_box_append(GTK_BOX(box), gtk_label_new("Message (hide) / Revealed text (extract):"));
    GtkWidget *tv_sw, *tv;
    tv_sw = hv_make_text_view(&tv, TRUE);
    gtk_widget_set_size_request(tv_sw, -1, 100);
    gtk_box_append(GTK_BOX(box), tv_sw);
    GtkWidget *btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *hide_btn   = hv_make_action_btn("Hide Message");
    GtkWidget *reveal_btn = gtk_button_new_with_label("Extract Message");
    gtk_box_append(GTK_BOX(btn_row), hide_btn);
    gtk_box_append(GTK_BOX(btn_row), reveal_btn);
    gtk_box_append(GTK_BOX(box), btn_row);
    GtkWidget *status = hv_make_result_label();
    StegCtx *ctx = g_new0(StegCtx, 1);
    ctx->in_e=in_e; ctx->out_e=out_e; ctx->msg_tv=tv; ctx->status=status;
    g_signal_connect(hide_btn,   "clicked", G_CALLBACK(on_steg_hide),   ctx);
    g_signal_connect(reveal_btn, "clicked", G_CALLBACK(on_steg_reveal), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);
    gtk_box_append(GTK_BOX(box), status);
    return box;
}

/* ------------------------------------------------------------------ */
/* Image to ASCII Art */
static const char *ascii_chars = " .,:;i1tfLCG08@";
static const int   ascii_chars_len = 15;

static void on_ascii_art(GtkButton *btn, gpointer ud) {
    (void)btn;
    GtkWidget *tv = ud;
    GtkWidget *entry = g_object_get_data(G_OBJECT(btn), "path-entry");
    GtkWidget *cols_sp = g_object_get_data(G_OBJECT(btn), "cols-spin");
    const char *path = gtk_editable_get_text(GTK_EDITABLE(entry));
    if (!path || !*path) return;
    int cols = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(cols_sp));

    GError *err = NULL;
    /* Scale to cols width, aspect ratio ~ 0.5 (characters are taller than wide) */
    GdkPixbuf *src = gdk_pixbuf_new_from_file(path, &err);
    if (!src) { hv_textview_set_text(tv, err ? err->message : "Load failed");
                if (err) g_error_free(err); return; }
    int ow = gdk_pixbuf_get_width(src), oh = gdk_pixbuf_get_height(src);
    int rows = MAX(1, (int)((double)oh / ow * cols * 0.45));
    GdkPixbuf *pb = gdk_pixbuf_scale_simple(src, cols, rows, GDK_INTERP_BILINEAR);
    g_object_unref(src);

    GdkPixbuf *gray_src = pb;
    GdkPixbuf *gray = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, cols, rows);
    /* manual grayscale conversion */
    int rs_in  = gdk_pixbuf_get_rowstride(gray_src);
    int nc_in  = gdk_pixbuf_get_n_channels(gray_src);
    int rs_out = gdk_pixbuf_get_rowstride(gray);
    guchar *pix_in  = gdk_pixbuf_get_pixels(gray_src);
    guchar *pix_out = gdk_pixbuf_get_pixels(gray);
    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            guchar *pi = pix_in  + y * rs_in  + x * nc_in;
            guchar *po = pix_out + y * rs_out + x * 3;
            guchar lum = (guchar)(0.299*pi[0] + 0.587*pi[1] + 0.114*pi[2]);
            po[0] = po[1] = po[2] = lum;
        }
    }
    g_object_unref(pb);

    GString *art = g_string_new(NULL);
    guchar *gpix = gdk_pixbuf_get_pixels(gray);
    int grs = gdk_pixbuf_get_rowstride(gray);
    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            guchar lum = *(gpix + y * grs + x * 3);
            int idx = (int)(lum / 255.0 * (ascii_chars_len - 1));
            g_string_append_c(art, ascii_chars[idx]);
        }
        g_string_append_c(art, '\n');
    }
    g_object_unref(gray);
    hv_textview_set_text(tv, art->str);
    g_string_free(art, TRUE);
}

GtkWidget *build_image_to_ascii_art(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *in_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Image file:", &in_e, FALSE));
    GtkWidget *cols_sp;
    gtk_box_append(GTK_BOX(box), make_spin_row("Width (columns):", 20, 200, 5, 80, &cols_sp));
    GtkWidget *btn = hv_make_action_btn("Convert to ASCII Art");
    GtkWidget *tv_sw, *tv;
    tv_sw = hv_make_text_view(&tv, FALSE);
    gtk_widget_set_vexpand(tv_sw, TRUE);
    g_object_set_data(G_OBJECT(btn), "path-entry", in_e);
    g_object_set_data(G_OBJECT(btn), "cols-spin",  cols_sp);
    g_signal_connect(btn, "clicked", G_CALLBACK(on_ascii_art), tv);
    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), tv_sw);
    return box;
}

/* ------------------------------------------------------------------ */
/* Histogram (channel stats via identify + simple bar drawing) */
static void on_histogram_load(GtkButton *btn, gpointer ud) {
    (void)btn;
    GtkWidget *tv = ud;
    GtkWidget *entry = g_object_get_data(G_OBJECT(btn), "path-entry");
    const char *path = gtk_editable_get_text(GTK_EDITABLE(entry));
    if (!path || !*path) return;
    char *qp = g_shell_quote(path);
    char *cmd = g_strdup_printf(
        "identify -verbose %s 2>&1 | grep -A50 'Channel statistics' | head -60", qp);
    g_free(qp);
    char *res = hv_run_cmd(cmd); g_free(cmd);
    hv_textview_set_text(tv, res ? res : "No stats");
    g_free(res);
}

GtkWidget *build_image_histogram(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *in_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Image file:", &in_e, FALSE));
    GtkWidget *btn = hv_make_action_btn("Analyze Histogram");
    GtkWidget *tv_sw, *tv;
    tv_sw = hv_make_text_view(&tv, FALSE);
    gtk_widget_set_vexpand(tv_sw, TRUE);
    g_object_set_data(G_OBJECT(btn), "path-entry", in_e);
    g_signal_connect(btn, "clicked", G_CALLBACK(on_histogram_load), tv);
    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), tv_sw);
    return box;
}

/* ------------------------------------------------------------------ */
/* Screenshot Tool */
static void on_screenshot(GtkButton *btn, gpointer ud) {
    (void)btn;
    GtkWidget *status = ud;
    GtkWidget *out_e = g_object_get_data(G_OBJECT(btn), "out-entry");
    GtkWidget *delay_sp = g_object_get_data(G_OBJECT(btn), "delay-spin");
    const char *out = gtk_editable_get_text(GTK_EDITABLE(out_e));
    if (!out || !*out) { gtk_label_set_text(GTK_LABEL(status), "⚠ Set output path"); return; }
    int delay = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(delay_sp));
    char *qo = g_shell_quote(out);
    char *cmd = g_strdup_printf("sleep %d && import -window root %s 2>&1", delay, qo);
    g_free(qo);
    gtk_label_set_text(GTK_LABEL(status), "📸 Taking screenshot...");
    char *res = hv_run_cmd(cmd); g_free(cmd);
    gtk_label_set_text(GTK_LABEL(status),
        (!res||!*res) ? "✓ Screenshot saved!" : res);
    g_free(res);
}

GtkWidget *build_image_screenshot_tool(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *out_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Save as:", &out_e, TRUE));
    GtkWidget *delay_sp;
    gtk_box_append(GTK_BOX(box), make_spin_row("Delay (seconds):", 0, 30, 1, 3, &delay_sp));
    GtkWidget *btn = hv_make_action_btn("Take Screenshot");
    GtkWidget *status = hv_make_result_label();
    g_object_set_data(G_OBJECT(btn), "out-entry",  out_e);
    g_object_set_data(G_OBJECT(btn), "delay-spin", delay_sp);
    g_signal_connect(btn, "clicked", G_CALLBACK(on_screenshot), status);
    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), status);
    return box;
}

/* ------------------------------------------------------------------ */
/* Animated GIF Maker */
typedef struct { GtkWidget *folder_e, *out_e, *delay_sp, *status; } GifMakerCtx;
static void on_gif_make(GtkButton *btn, gpointer ud) {
    (void)btn;
    GifMakerCtx *ctx = ud;
    const char *folder = gtk_editable_get_text(GTK_EDITABLE(ctx->folder_e));
    const char *out    = gtk_editable_get_text(GTK_EDITABLE(ctx->out_e));
    if (!folder||!*folder||!out||!*out) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Fill both fields"); return;
    }
    int delay = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->delay_sp));
    char *qf = g_shell_quote(folder);
    char *qo = g_shell_quote(out);
    char *cmd = g_strdup_printf(
        "convert -delay %d -loop 0 %s/*.png %s/*.jpg %s 2>&1 | head -5",
        delay, qf, qf, qo);
    g_free(qf); g_free(qo);
    char *res = hv_run_cmd(cmd); g_free(cmd);
    gtk_label_set_text(GTK_LABEL(ctx->status),
        (!res||!*res||strstr(res,"WARNING")==res) ? "✓ GIF created!" : res);
    g_free(res);
}

GtkWidget *build_image_animated_gif_maker(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *folder_e, *out_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Frames folder:", &folder_e, FALSE));
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output GIF:", &out_e, TRUE));
    GtkWidget *delay_sp;
    gtk_box_append(GTK_BOX(box), make_spin_row("Frame delay (1/100s):", 1, 500, 1, 10, &delay_sp));
    GtkWidget *btn = hv_make_action_btn("Create Animated GIF");
    GtkWidget *status = hv_make_result_label();
    GifMakerCtx *ctx = g_new0(GifMakerCtx, 1);
    ctx->folder_e=folder_e; ctx->out_e=out_e; ctx->delay_sp=delay_sp; ctx->status=status;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_gif_make), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);
    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), status);
    return box;
}

/* ------------------------------------------------------------------ */
/* Animated GIF Splitter */
static void on_gif_split(GtkButton *btn, gpointer ud) {
    (void)btn;
    GtkWidget *status = ud;
    GtkWidget *in_e   = g_object_get_data(G_OBJECT(btn), "in-entry");
    GtkWidget *out_e  = g_object_get_data(G_OBJECT(btn), "out-entry");
    const char *in  = gtk_editable_get_text(GTK_EDITABLE(in_e));
    const char *out = gtk_editable_get_text(GTK_EDITABLE(out_e));
    if (!in||!*in||!out||!*out) {
        gtk_label_set_text(GTK_LABEL(status), "⚠ Fill both paths"); return;
    }
    /* out should be a pattern like /path/frame_%04d.png */
    char *qi = g_shell_quote(in);
    char *cmd = g_strdup_printf("convert %s '%s/frame_%%04d.png' 2>&1", qi, out);
    g_free(qi);
    char *res = hv_run_cmd(cmd); g_free(cmd);
    gtk_label_set_text(GTK_LABEL(status),
        (!res||!*res||strstr(res,"WARNING")==res) ? "✓ GIF split into frames!" : res);
    g_free(res);
}

GtkWidget *build_image_animated_gif_splitter(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *in_e, *out_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input GIF:", &in_e, FALSE));
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output folder:", &out_e, FALSE));
    GtkWidget *note = gtk_label_new("Frames will be saved as frame_0000.png, frame_0001.png, …");
    gtk_widget_add_css_class(note, "helvetia-fg-muted");
    gtk_widget_set_halign(note, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box), note);
    GtkWidget *btn = hv_make_action_btn("Split GIF");
    GtkWidget *status = hv_make_result_label();
    g_object_set_data(G_OBJECT(btn), "in-entry",  in_e);
    g_object_set_data(G_OBJECT(btn), "out-entry", out_e);
    g_signal_connect(btn, "clicked", G_CALLBACK(on_gif_split), status);
    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), status);
    return box;
}

/* ------------------------------------------------------------------ */
/* Text Tool */
typedef struct { GtkWidget *in_e, *out_e, *text_e, *font_e, *size_sp, *color_e, *grav_dd, *status; } TextToolCtx;
static void on_text_tool(GtkButton *btn, gpointer ud) {
    (void)btn;
    TextToolCtx *ctx = ud;
    const char *in   = gtk_editable_get_text(GTK_EDITABLE(ctx->in_e));
    const char *out  = gtk_editable_get_text(GTK_EDITABLE(ctx->out_e));
    const char *text = gtk_editable_get_text(GTK_EDITABLE(ctx->text_e));
    const char *font = gtk_editable_get_text(GTK_EDITABLE(ctx->font_e));
    const char *col  = gtk_editable_get_text(GTK_EDITABLE(ctx->color_e));
    if (!in||!*in||!out||!*out||!text||!*text) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Fill required fields"); return;
    }
    int size = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->size_sp));
    static const char *gravities[] = {"NorthWest","North","NorthEast","West","Center","East","SouthWest","South","SouthEast"};
    guint gi = gtk_drop_down_get_selected(GTK_DROP_DOWN(ctx->grav_dd));
    const char *grav = gravities[gi < 9 ? gi : 4];
    if (!col||!*col) col = "white";
    if (!font||!*font) font = "DejaVu-Sans";
    char *qi = g_shell_quote(in), *qo = g_shell_quote(out), *qt = g_shell_quote(text);
    char *cmd = g_strdup_printf(
        "convert %s -font %s -pointsize %d -fill '%s' -gravity %s -annotate 0 %s %s 2>&1",
        qi, font, size, col, grav, qt, qo);
    g_free(qi); g_free(qo); g_free(qt);
    char *res = hv_run_cmd(cmd); g_free(cmd);
    gtk_label_set_text(GTK_LABEL(ctx->status),
        (!res||!*res||strstr(res,"WARNING")==res) ? "✓ Text applied!" : res);
    g_free(res);
}

GtkWidget *build_image_text_tool(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *in_e, *out_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:", &in_e, FALSE));
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output image:", &out_e, TRUE));
    GtkWidget *text_e; gtk_box_append(GTK_BOX(box), hv_make_entry_row("Text:", &text_e));
    GtkWidget *font_e; gtk_box_append(GTK_BOX(box), hv_make_entry_row("Font:", &font_e));
    gtk_entry_set_placeholder_text(GTK_ENTRY(font_e), "DejaVu-Sans");
    GtkWidget *color_e; gtk_box_append(GTK_BOX(box), hv_make_entry_row("Color:", &color_e));
    gtk_entry_set_placeholder_text(GTK_ENTRY(color_e), "white");
    GtkWidget *size_sp;
    gtk_box_append(GTK_BOX(box), make_spin_row("Font size (pt):", 6, 300, 2, 36, &size_sp));
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(row), gtk_label_new("Gravity:"));
    const char *gravs[] = {"NW","N","NE","W","Center","E","SW","S","SE",NULL};
    GtkWidget *grav_dd = gtk_drop_down_new_from_strings(gravs);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(grav_dd), 4);
    gtk_box_append(GTK_BOX(row), grav_dd);
    gtk_box_append(GTK_BOX(box), row);
    GtkWidget *btn = hv_make_action_btn("Add Text");
    GtkWidget *status = hv_make_result_label();
    TextToolCtx *ctx = g_new0(TextToolCtx, 1);
    ctx->in_e=in_e; ctx->out_e=out_e; ctx->text_e=text_e;
    ctx->font_e=font_e; ctx->size_sp=size_sp; ctx->color_e=color_e;
    ctx->grav_dd=grav_dd; ctx->status=status;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_text_tool), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);
    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), status);
    return box;
}

/* ------------------------------------------------------------------ */
/* Shape Tool */
typedef struct { GtkWidget *in_e, *out_e, *shape_dd, *x_sp, *y_sp, *w_sp, *h_sp, *color_e, *fill_e, *thick_sp, *status; } ShapeCtx;
static void on_shape_draw(GtkButton *btn, gpointer ud) {
    (void)btn;
    ShapeCtx *ctx = ud;
    const char *in  = gtk_editable_get_text(GTK_EDITABLE(ctx->in_e));
    const char *out = gtk_editable_get_text(GTK_EDITABLE(ctx->out_e));
    const char *stroke = gtk_editable_get_text(GTK_EDITABLE(ctx->color_e));
    const char *fill   = gtk_editable_get_text(GTK_EDITABLE(ctx->fill_e));
    if (!in||!*in||!out||!*out) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Fill paths"); return;
    }
    if (!stroke||!*stroke) stroke = "red";
    if (!fill||!*fill)     fill   = "none";
    int x = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->x_sp));
    int y = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->y_sp));
    int w = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->w_sp));
    int h = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->h_sp));
    int t = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->thick_sp));
    guint si = gtk_drop_down_get_selected(GTK_DROP_DOWN(ctx->shape_dd));
    char shape_str[128];
    if (si == 0) /* Rectangle */
        snprintf(shape_str, sizeof shape_str, "rectangle %d,%d %d,%d", x, y, x+w, y+h);
    else if (si == 1) /* Ellipse */
        snprintf(shape_str, sizeof shape_str, "ellipse %d,%d %d,%d 0,360", x, y, w/2, h/2);
    else /* Line */
        snprintf(shape_str, sizeof shape_str, "line %d,%d %d,%d", x, y, x+w, y+h);
    char *qi = g_shell_quote(in), *qo = g_shell_quote(out);
    char *cmd = g_strdup_printf(
        "convert %s -fill '%s' -stroke '%s' -strokewidth %d -draw '%s' %s 2>&1",
        qi, fill, stroke, t, shape_str, qo);
    g_free(qi); g_free(qo);
    char *res = hv_run_cmd(cmd); g_free(cmd);
    gtk_label_set_text(GTK_LABEL(ctx->status),
        (!res||!*res||strstr(res,"WARNING")==res) ? "✓ Shape drawn!" : res);
    g_free(res);
}

GtkWidget *build_image_shape_tool(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *in_e, *out_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:", &in_e, FALSE));
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output image:", &out_e, TRUE));
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(row), gtk_label_new("Shape:"));
    const char *shapes[] = {"Rectangle","Ellipse","Line",NULL};
    GtkWidget *shape_dd = gtk_drop_down_new_from_strings(shapes);
    gtk_box_append(GTK_BOX(row), shape_dd);
    gtk_box_append(GTK_BOX(box), row);
    GtkWidget *x_sp, *y_sp, *w_sp, *h_sp, *t_sp;
    gtk_box_append(GTK_BOX(box), make_spin_row("X:", 0, 65535, 1, 10, &x_sp));
    gtk_box_append(GTK_BOX(box), make_spin_row("Y:", 0, 65535, 1, 10, &y_sp));
    gtk_box_append(GTK_BOX(box), make_spin_row("Width:", 1, 65535, 1, 100, &w_sp));
    gtk_box_append(GTK_BOX(box), make_spin_row("Height:", 1, 65535, 1, 100, &h_sp));
    gtk_box_append(GTK_BOX(box), make_spin_row("Stroke width:", 1, 50, 1, 2, &t_sp));
    GtkWidget *color_e, *fill_e;
    gtk_box_append(GTK_BOX(box), hv_make_entry_row("Stroke color:", &color_e));
    gtk_entry_set_placeholder_text(GTK_ENTRY(color_e), "red");
    gtk_box_append(GTK_BOX(box), hv_make_entry_row("Fill color:", &fill_e));
    gtk_entry_set_placeholder_text(GTK_ENTRY(fill_e), "none");
    GtkWidget *btn = hv_make_action_btn("Draw Shape");
    GtkWidget *status = hv_make_result_label();
    ShapeCtx *ctx = g_new0(ShapeCtx, 1);
    ctx->in_e=in_e; ctx->out_e=out_e; ctx->shape_dd=shape_dd;
    ctx->x_sp=x_sp; ctx->y_sp=y_sp; ctx->w_sp=w_sp; ctx->h_sp=h_sp;
    ctx->color_e=color_e; ctx->fill_e=fill_e; ctx->thick_sp=t_sp; ctx->status=status;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_shape_draw), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);
    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), status);
    return box;
}

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

    char *cmd = g_strdup_printf("convert %s -crop %s +repage %s 2>&1", qi, qg, qo);
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

/* ================================================================
 * Generic single-param ImageMagick tools (macro-built below)
 * ================================================================ */

/* Generic 1-path-in 1-path-out + IM args builder */
typedef struct {
    GtkWidget *in_e, *out_e, *status;
    GtkWidget *p0, *p1, *p2, *p3;
    char *(*get_args)(void *);
} Gen1Ctx;

static void on_gen1_run(GtkButton *btn, gpointer ud) {
    (void)btn;
    Gen1Ctx *ctx = ud;
    const char *in  = gtk_editable_get_text(GTK_EDITABLE(ctx->in_e));
    const char *out = gtk_editable_get_text(GTK_EDITABLE(ctx->out_e));
    if (!in||!*in||!out||!*out) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Fill both paths"); return;
    }
    char *args = ctx->get_args(ctx);
    char *qi = g_shell_quote(in), *qo = g_shell_quote(out);
    char *cmd = g_strdup_printf("convert %s %s %s 2>&1", qi, args ? args : "", qo);
    g_free(qi); g_free(qo); g_free(args);
    char *res = hv_run_cmd(cmd); g_free(cmd);
    gtk_label_set_text(GTK_LABEL(ctx->status),
        (!res||!*res||strstr(res,"WARNING")==res) ? "✓ Done!" : res);
    g_free(res);
}

/* Helper macro: build_image_XXX with custom UI + args function */
#define MAKE_IM_TOOL(FUNCNAME, BTNLABEL, ARGSFUNC, UICODE) \
GtkWidget *FUNCNAME(void) { \
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10); \
    GtkWidget *in_e, *out_e; \
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:", &in_e, FALSE)); \
    UICODE \
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output image:", &out_e, TRUE)); \
    GtkWidget *btn = hv_make_action_btn(BTNLABEL); \
    GtkWidget *status = hv_make_result_label(); \
    Gen1Ctx *ctx = g_new0(Gen1Ctx, 1); \
    ctx->in_e=in_e; ctx->out_e=out_e; ctx->status=status; \
    ctx->get_args=(char*(*)(void*))ARGSFUNC; \
    g_signal_connect(btn,"clicked",G_CALLBACK(on_gen1_run),ctx); \
    g_signal_connect_swapped(box,"destroy",G_CALLBACK(g_free),ctx); \
    gtk_box_append(GTK_BOX(box), btn); \
    gtk_box_append(GTK_BOX(box), status); \
    return box; \
}

/* --- Adjust args functions using Gen1Ctx --- */

static char *_args_rotate_g(Gen1Ctx *ctx) {
    double a = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p0));
    return g_strdup_printf("-rotate %.1f", a);
}
static char *_args_straighten_g(Gen1Ctx *ctx) {
    double a = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p0));
    return g_strdup_printf("-rotate %.2f -gravity Center", a);
}
static char *_args_scale_g(Gen1Ctx *ctx) {
    int w=(int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p0));
    int h=(int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p1));
    return g_strdup_printf("-scale %dx%d!", w, h);
}
static char *_args_skew_g(Gen1Ctx *ctx) {
    double sx=gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p0));
    double sy=gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p1));
    return g_strdup_printf("-shear %.1fx%.1f", sx, sy);
}
static char *_args_distort_g(Gen1Ctx *ctx) {
    double deg=gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p0));
    guint t=gtk_drop_down_get_selected(GTK_DROP_DOWN(ctx->p1));
    static const char *dt[]={"Arc","Barrel","Polar","ScaleRotateTranslate"};
    return g_strdup_printf("-distort %s '%.1f'", dt[t<4?t:0], deg);
}
static char *_args_warp_g(Gen1Ctx *ctx) {
    double sw=gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p0));
    double wa=gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p1));
    double wl=gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p2));
    return g_strdup_printf("-swirl %.0f -wave %.0fx%.0f", sw, wa, wl);
}
static char *_args_bc_g(Gen1Ctx *ctx) {
    double b=gtk_range_get_value(GTK_RANGE(ctx->p0));
    double c=gtk_range_get_value(GTK_RANGE(ctx->p1));
    return g_strdup_printf("-brightness-contrast %.0fx%.0f", b, c);
}
static char *_args_levels_g(Gen1Ctx *ctx) {
    double bl=gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p0));
    double wh=gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p1));
    double gm=gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p2));
    return g_strdup_printf("-level '%.0f%%,%.0f%%,%.2f'", bl, wh, gm);
}
static char *_args_exposure_g(Gen1Ctx *ctx) {
    double ev=gtk_range_get_value(GTK_RANGE(ctx->p0));
    return g_strdup_printf("-evaluate Multiply %.4f", pow(2.0,ev));
}
static char *_args_sat_g(Gen1Ctx *ctx) {
    double s=gtk_range_get_value(GTK_RANGE(ctx->p0));
    return g_strdup_printf("-modulate 100,%.0f,100", 100.0+s);
}
static char *_args_hue_g(Gen1Ctx *ctx) {
    double h=gtk_range_get_value(GTK_RANGE(ctx->p0));
    return g_strdup_printf("-modulate 100,100,%.0f", 100.0+h/1.8);
}
static char *_args_wb_g(Gen1Ctx *ctx) {
    double t=gtk_range_get_value(GTK_RANGE(ctx->p0));
    double rf=CLAMP(1.0+t/200.0,0.5,2.0), bf=CLAMP(1.0-t/200.0,0.5,2.0);
    return g_strdup_printf("-channel R -evaluate Multiply %.3f -channel B -evaluate Multiply %.3f +channel", rf, bf);
}
static char *_args_cb_g(Gen1Ctx *ctx) {
    double r=gtk_range_get_value(GTK_RANGE(ctx->p0));
    double g=gtk_range_get_value(GTK_RANGE(ctx->p1));
    double b=gtk_range_get_value(GTK_RANGE(ctx->p2));
    return g_strdup_printf("-channel R -evaluate Add %.0f -channel G -evaluate Add %.0f -channel B -evaluate Add %.0f +channel", r, g, b);
}
static char *_args_sh_g(Gen1Ctx *ctx) {
    double s=gtk_range_get_value(GTK_RANGE(ctx->p0));
    double h=gtk_range_get_value(GTK_RANGE(ctx->p1));
    return g_strdup_printf("-level '%.0f%%,%.0f%%'", s, 100.0-h);
}
static char *_args_gamma_g(Gen1Ctx *ctx) {
    double g=gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p0));
    return g_strdup_printf("-gamma %.2f", g);
}
static char *_args_autoenh_g(Gen1Ctx *ctx) { (void)ctx; return g_strdup("-auto-level -auto-gamma"); }
static char *_args_sharpen_g(Gen1Ctx *ctx) {
    double r=gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p0));
    double s=gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p1));
    return g_strdup_printf("-sharpen %.1fx%.1f", r, s);
}
static char *_args_unsharp_g(Gen1Ctx *ctx) {
    double r=gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p0));
    double s=gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p1));
    double a=gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p2));
    double t=gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p3));
    return g_strdup_printf("-unsharp %.1fx%.1f+%.2f+%.2f", r, s, a, t);
}
static char *_args_noise_g(Gen1Ctx *ctx) {
    int n=(int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p0));
    GString *st=g_string_new(NULL);
    for(int i=0;i<n;i++) g_string_append(st,"-despeckle ");
    return g_string_free(st,FALSE);
}
static char *_args_denoise_g(Gen1Ctx *ctx) { (void)ctx; return g_strdup("-enhance -enhance"); }
static char *_args_posterize_g(Gen1Ctx *ctx) {
    int l=(int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p0));
    return g_strdup_printf("-posterize %d", l);
}
static char *_args_vignette_g(Gen1Ctx *ctx) {
    double s=gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p0));
    double x=gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p1));
    double y=gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p2));
    return g_strdup_printf("-vignette %.0fx%.0f+%.0f+%.0f", s*2, s, x, y);
}
static char *_args_glow_g(Gen1Ctx *ctx) {
    double s=gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p0));
    return g_strdup_printf("\\( +clone -blur 0x%.1f \\) -compose Screen -composite", s);
}
static char *_args_emboss_g(Gen1Ctx *ctx) {
    double s=gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p0));
    return g_strdup_printf("-emboss %.1f", s);
}
static char *_args_edge_g(Gen1Ctx *ctx) {
    double r=gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p0));
    return g_strdup_printf("-edge %.1f", r);
}
static char *_args_mosaic_g(Gen1Ctx *ctx) {
    int p=(int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p0));
    return g_strdup_printf("-scale %d%% -scale 10000%%", p);
}
static char *_args_bgrem_g(Gen1Ctx *ctx) {
    const char *c=gtk_editable_get_text(GTK_EDITABLE(ctx->p0));
    double f=gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p1));
    if(!c||!*c) c="white";
    return g_strdup_printf("-fuzz %.0f%% -transparent '%s'", f, c);
}
static char *_args_portrait_g(Gen1Ctx *ctx) { (void)ctx; return g_strdup("-unsharp 0x1+0.5+0 -modulate 100,110,100 -auto-level"); }
static char *_args_blemish_g(Gen1Ctx *ctx) {
    int x=(int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p0));
    int y=(int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p1));
    int r=(int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p2));
    return g_strdup_printf("-region %dx%d+%d+%d -paint %d +region", r*2, r*2, x-r, y-r, r);
}
static char *_args_teeth_g(Gen1Ctx *ctx) {
    int x=(int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p0));
    int y=(int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p1));
    int w=(int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p2));
    int h=(int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p3));
    return g_strdup_printf("-region %dx%d+%d+%d -modulate 105,70,100 +region", w, h, x, y);
}
static char *_args_skin_g(Gen1Ctx *ctx) {
    double s=gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p0));
    return g_strdup_printf("-blur 0x%.1f -unsharp 0x1+0.3+0", s);
}
static char *_args_panorama_g(Gen1Ctx *ctx) { (void)ctx; return g_strdup("+append"); }
static char *_args_hdr_g(Gen1Ctx *ctx) { (void)ctx; return g_strdup("-evaluate-sequence Mean"); }
static char *_args_focus_g(Gen1Ctx *ctx) { (void)ctx; return g_strdup("-evaluate-sequence Max"); }
static char *_args_border_g(Gen1Ctx *ctx) {
    int w=(int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p0));
    const char *c=gtk_editable_get_text(GTK_EDITABLE(ctx->p1));
    if(!c||!*c) c="black";
    return g_strdup_printf("-bordercolor '%s' -border %d", c, w);
}
static char *_args_shadow_g(Gen1Ctx *ctx) {
    double s=gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p0));
    int xo=(int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p1));
    int yo=(int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p2));
    return g_strdup_printf("\\( +clone -background black -shadow 80x%.0f+%d+%d \\) +swap -background none -layers merge", s, xo, yo);
}
static char *_args_reflect_g(Gen1Ctx *ctx) {
    double alpha=gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p0));
    return g_strdup_printf("-flip -alpha set -channel Alpha -evaluate Multiply %.2f +channel", alpha/100.0);
}
static char *_args_watermark_g(Gen1Ctx *ctx) {
    const char *t=gtk_editable_get_text(GTK_EDITABLE(ctx->p0));
    double a=gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p1));
    guint gi=gtk_drop_down_get_selected(GTK_DROP_DOWN(ctx->p2));
    static const char *gravs[]={"SouthEast","South","Center","NorthEast","North"};
    const char *grav=gravs[gi<5?gi:0];
    if(!t||!*t) t="Watermark";
    return g_strdup_printf("-gravity %s -fill \"rgba(255,255,255,%.2f)\" -pointsize 36 -annotate 0 '%s'",
                           grav, a/100.0, t);
}
static char *_args_pxart_g(Gen1Ctx *ctx) {
    int s=(int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p0));
    return g_strdup_printf("-filter point -resize %d%%", s);
}
static char *_args_compress_g(Gen1Ctx *ctx) {
    int q=(int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p0));
    return g_strdup_printf("-quality %d", q);
}
static char *_args_lossless_g(Gen1Ctx *ctx) {
    (void)ctx;
    return g_strdup("-define png:compression-level=9 -define png:compression-strategy=1 -strip");
}
static char *_args_svg2png_g(Gen1Ctx *ctx) {
    int d=(int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p0));
    return g_strdup_printf("-density %d -background none", d);
}
static char *_args_opacity_g(Gen1Ctx *ctx) {
    double p=gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p0));
    return g_strdup_printf("-alpha set -channel Alpha -evaluate Multiply %.3f +channel", p/100.0);
}
static char *_args_autocrop_g(Gen1Ctx *ctx) {
    double f=gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p0));
    return g_strdup_printf("-fuzz %.0f%% -trim +repage", f);
}
static char *_args_canvas_g(Gen1Ctx *ctx) {
    int w=(int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p0));
    int h=(int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->p1));
    guint gi=gtk_drop_down_get_selected(GTK_DROP_DOWN(ctx->p2));
    static const char *gravs[]={"NorthWest","North","NorthEast","West","Center","East","SouthWest","South","SouthEast"};
    return g_strdup_printf("-gravity %s -extent %dx%d", gravs[gi<9?gi:4], w, h);
}
static char *_args_perspective_g(Gen1Ctx *ctx) {
    const char *c=gtk_editable_get_text(GTK_EDITABLE(ctx->p0));
    return g_strdup_printf("-distort Perspective '%s'", c ? c : "0,0,0,0");
}

/* ================================================================
 * Now define all the individual build_ functions using the generic runner
 * ================================================================ */

/* Helper to quickly build a Gen1Ctx + connect it */
static GtkWidget *_finish_gen1(GtkWidget *box, GtkWidget *in_e, GtkWidget *out_e,
                                const char *btn_label,
                                char *(*fn)(Gen1Ctx*),
                                GtkWidget *p0, GtkWidget *p1,
                                GtkWidget *p2, GtkWidget *p3) {
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output image:", &out_e, TRUE));
    GtkWidget *btn = hv_make_action_btn(btn_label);
    GtkWidget *status = hv_make_result_label();
    Gen1Ctx *ctx = g_new0(Gen1Ctx, 1);
    ctx->in_e=in_e; ctx->out_e=out_e; ctx->status=status;
    ctx->p0=p0; ctx->p1=p1; ctx->p2=p2; ctx->p3=p3;
    ctx->get_args=(char*(*)(void*))fn;
    g_signal_connect(btn,"clicked",G_CALLBACK(on_gen1_run),ctx);
    g_signal_connect_swapped(box,"destroy",G_CALLBACK(g_free),ctx);
    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), status);
    return box;
}

GtkWidget *build_image_rotate(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:",&in_e,FALSE));
    GtkWidget *sp; gtk_box_append(GTK_BOX(box), make_spin_row("Angle (°):",-360,360,0.5,90,&sp));
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Rotate Image",_args_rotate_g,sp,NULL,NULL,NULL);
}

GtkWidget *build_image_straighten(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:",&in_e,FALSE));
    GtkWidget *sp; gtk_box_append(GTK_BOX(box), make_spin_row("Straighten angle (°):",-45,45,0.1,0,&sp));
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Straighten",_args_straighten_g,sp,NULL,NULL,NULL);
}

GtkWidget *build_image_perspective_correct(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:",&in_e,FALSE));
    GtkWidget *coord_e; gtk_box_append(GTK_BOX(box), hv_make_entry_row("Perspective coords:",&coord_e));
    gtk_entry_set_placeholder_text(GTK_ENTRY(coord_e),"0,0,0,0 100,0,90,10 0,100,10,90 100,100,100,100");
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Apply Perspective",_args_perspective_g,coord_e,NULL,NULL,NULL);
}

GtkWidget *build_image_canvas_resize(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:",&in_e,FALSE));
    GtkWidget *w_sp, *h_sp;
    gtk_box_append(GTK_BOX(box), make_spin_row("New width:", 1, 8192, 10, 1920, &w_sp));
    gtk_box_append(GTK_BOX(box), make_spin_row("New height:", 1, 8192, 10, 1080, &h_sp));
    GtkWidget *row=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8);
    gtk_box_append(GTK_BOX(row), gtk_label_new("Anchor:"));
    const char *gravs[]={"NW","N","NE","W","Center","E","SW","S","SE",NULL};
    GtkWidget *grav_dd=gtk_drop_down_new_from_strings(gravs);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(grav_dd),4);
    gtk_box_append(GTK_BOX(row),grav_dd);
    gtk_box_append(GTK_BOX(box),row);
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Resize Canvas",_args_canvas_g,w_sp,h_sp,grav_dd,NULL);
}

GtkWidget *build_image_auto_crop_borders(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:",&in_e,FALSE));
    GtkWidget *sp; gtk_box_append(GTK_BOX(box), make_spin_row("Fuzz %:", 0, 50, 1, 5, &sp));
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Auto-crop Borders",_args_autocrop_g,sp,NULL,NULL,NULL);
}

GtkWidget *build_image_brightness_contrast(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:",&in_e,FALSE));
    GtkWidget *b_sl, *c_sl;
    gtk_box_append(GTK_BOX(box), make_scale_row("Brightness (-100..+100):", -100, 100, 0, &b_sl));
    gtk_box_append(GTK_BOX(box), make_scale_row("Contrast (-100..+100):", -100, 100, 0, &c_sl));
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Apply Brightness/Contrast",_args_bc_g,b_sl,c_sl,NULL,NULL);
}

GtkWidget *build_image_levels(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:",&in_e,FALSE));
    GtkWidget *bl, *wh, *gm;
    gtk_box_append(GTK_BOX(box), make_spin_row("Black point (%):", 0, 49, 1, 0, &bl));
    gtk_box_append(GTK_BOX(box), make_spin_row("White point (%):", 51, 100, 1, 100, &wh));
    gtk_box_append(GTK_BOX(box), make_spin_row("Gamma:", 0.1, 5.0, 0.1, 1.0, &gm));
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Apply Levels",_args_levels_g,bl,wh,gm,NULL);
}

GtkWidget *build_image_curves(void) {
    /* Curves: simplified as Levels for now */
    return build_image_levels();
}

GtkWidget *build_image_exposure(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:",&in_e,FALSE));
    GtkWidget *sl;
    gtk_box_append(GTK_BOX(box), make_scale_row("Exposure (EV):", -4, 4, 0, &sl));
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Adjust Exposure",_args_exposure_g,sl,NULL,NULL,NULL);
}

GtkWidget *build_image_saturation_vibrance(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:",&in_e,FALSE));
    GtkWidget *sl;
    gtk_box_append(GTK_BOX(box), make_scale_row("Saturation (-100..+100):", -100, 100, 0, &sl));
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Adjust Saturation",_args_sat_g,sl,NULL,NULL,NULL);
}

GtkWidget *build_image_hue_shift(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:",&in_e,FALSE));
    GtkWidget *sl;
    gtk_box_append(GTK_BOX(box), make_scale_row("Hue shift (-180..+180°):", -180, 180, 0, &sl));
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Shift Hue",_args_hue_g,sl,NULL,NULL,NULL);
}

GtkWidget *build_image_white_balance(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:",&in_e,FALSE));
    GtkWidget *sl;
    gtk_box_append(GTK_BOX(box), make_scale_row("Temperature (cool ← → warm):", -100, 100, 0, &sl));
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Adjust White Balance",_args_wb_g,sl,NULL,NULL,NULL);
}

GtkWidget *build_image_color_balance(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:",&in_e,FALSE));
    GtkWidget *r_sl, *g_sl, *b_sl;
    gtk_box_append(GTK_BOX(box), make_scale_row("Red (-128..+128):",   -128, 128, 0, &r_sl));
    gtk_box_append(GTK_BOX(box), make_scale_row("Green (-128..+128):", -128, 128, 0, &g_sl));
    gtk_box_append(GTK_BOX(box), make_scale_row("Blue (-128..+128):",  -128, 128, 0, &b_sl));
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Adjust Color Balance",_args_cb_g,r_sl,g_sl,b_sl,NULL);
}

GtkWidget *build_image_shadows_highlights(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:",&in_e,FALSE));
    GtkWidget *s_sl, *h_sl;
    gtk_box_append(GTK_BOX(box), make_scale_row("Lift shadows (0-40%):", 0, 40, 0, &s_sl));
    gtk_box_append(GTK_BOX(box), make_scale_row("Compress highlights (0-30%):", 0, 30, 0, &h_sl));
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Adjust Shadows/Highlights",_args_sh_g,s_sl,h_sl,NULL,NULL);
}

GtkWidget *build_image_gamma_correction(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:",&in_e,FALSE));
    GtkWidget *sp; gtk_box_append(GTK_BOX(box), make_spin_row("Gamma:", 0.1, 5.0, 0.05, 1.0, &sp));
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Apply Gamma Correction",_args_gamma_g,sp,NULL,NULL,NULL);
}

GtkWidget *build_image_auto_enhance(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:",&in_e,FALSE));
    GtkWidget *note=gtk_label_new("Auto-level and auto-gamma will be applied.");
    gtk_widget_set_halign(note,GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box),note);
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Auto Enhance",_args_autoenh_g,NULL,NULL,NULL,NULL);
}

GtkWidget *build_image_sharpen(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:",&in_e,FALSE));
    GtkWidget *r_sp, *s_sp;
    gtk_box_append(GTK_BOX(box), make_spin_row("Radius:", 0, 10, 0.5, 0, &r_sp));
    gtk_box_append(GTK_BOX(box), make_spin_row("Sigma:", 0.1, 5, 0.1, 1.0, &s_sp));
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Sharpen",_args_sharpen_g,r_sp,s_sp,NULL,NULL);
}

GtkWidget *build_image_unsharp_mask(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:",&in_e,FALSE));
    GtkWidget *r_sp, *s_sp, *a_sp, *t_sp;
    gtk_box_append(GTK_BOX(box), make_spin_row("Radius:", 0, 10, 0.5, 0, &r_sp));
    gtk_box_append(GTK_BOX(box), make_spin_row("Sigma:", 0.1, 5, 0.1, 1.0, &s_sp));
    gtk_box_append(GTK_BOX(box), make_spin_row("Amount:", 0, 5, 0.1, 1.0, &a_sp));
    gtk_box_append(GTK_BOX(box), make_spin_row("Threshold:", 0, 0.1, 0.005, 0.05, &t_sp));
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Apply Unsharp Mask",_args_unsharp_g,r_sp,s_sp,a_sp,t_sp);
}

GtkWidget *build_image_noise_reduction(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:",&in_e,FALSE));
    GtkWidget *sp; gtk_box_append(GTK_BOX(box), make_spin_row("Despeckle passes:", 1, 6, 1, 2, &sp));
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Reduce Noise",_args_noise_g,sp,NULL,NULL,NULL);
}

GtkWidget *build_image_denoise(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:",&in_e,FALSE));
    GtkWidget *note=gtk_label_new("Two passes of IM -enhance will be applied.");
    gtk_widget_set_halign(note,GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box),note);
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Denoise",_args_denoise_g,NULL,NULL,NULL,NULL);
}

GtkWidget *build_image_posterize(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:",&in_e,FALSE));
    GtkWidget *sp; gtk_box_append(GTK_BOX(box), make_spin_row("Levels (2-8):", 2, 8, 1, 4, &sp));
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Posterize",_args_posterize_g,sp,NULL,NULL,NULL);
}

GtkWidget *build_image_vignette(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:",&in_e,FALSE));
    GtkWidget *s_sp, *x_sp, *y_sp;
    gtk_box_append(GTK_BOX(box), make_spin_row("Sigma:", 1, 80, 1, 20, &s_sp));
    gtk_box_append(GTK_BOX(box), make_spin_row("X offset:", -100, 100, 5, 0, &x_sp));
    gtk_box_append(GTK_BOX(box), make_spin_row("Y offset:", -100, 100, 5, 0, &y_sp));
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Apply Vignette",_args_vignette_g,s_sp,x_sp,y_sp,NULL);
}

GtkWidget *build_image_glow(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:",&in_e,FALSE));
    GtkWidget *sp; gtk_box_append(GTK_BOX(box), make_spin_row("Glow sigma:", 1, 30, 0.5, 4, &sp));
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Apply Glow",_args_glow_g,sp,NULL,NULL,NULL);
}

GtkWidget *build_image_emboss(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:",&in_e,FALSE));
    GtkWidget *sp; gtk_box_append(GTK_BOX(box), make_spin_row("Sigma:", 0.5, 10, 0.5, 1.0, &sp));
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Apply Emboss",_args_emboss_g,sp,NULL,NULL,NULL);
}

GtkWidget *build_image_edge_detect(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:",&in_e,FALSE));
    GtkWidget *sp; gtk_box_append(GTK_BOX(box), make_spin_row("Radius:", 0.5, 10, 0.5, 1.0, &sp));
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Detect Edges",_args_edge_g,sp,NULL,NULL,NULL);
}

GtkWidget *build_image_mosaic(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:",&in_e,FALSE));
    GtkWidget *sp; gtk_box_append(GTK_BOX(box), make_spin_row("Tile size (%): lower = bigger tiles", 1, 20, 1, 5, &sp));
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Apply Mosaic",_args_mosaic_g,sp,NULL,NULL,NULL);
}

GtkWidget *build_image_scale(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:",&in_e,FALSE));
    GtkWidget *w_sp, *h_sp;
    gtk_box_append(GTK_BOX(box), make_spin_row("Width:", 1, 8192, 10, 1920, &w_sp));
    gtk_box_append(GTK_BOX(box), make_spin_row("Height:", 1, 8192, 10, 1080, &h_sp));
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Scale Image",_args_scale_g,w_sp,h_sp,NULL,NULL);
}

GtkWidget *build_image_skew(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:",&in_e,FALSE));
    GtkWidget *sx_sp, *sy_sp;
    gtk_box_append(GTK_BOX(box), make_spin_row("Skew X (°):", -89, 89, 1, 0, &sx_sp));
    gtk_box_append(GTK_BOX(box), make_spin_row("Skew Y (°):", -89, 89, 1, 0, &sy_sp));
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Skew Image",_args_skew_g,sx_sp,sy_sp,NULL,NULL);
}

GtkWidget *build_image_distort(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:",&in_e,FALSE));
    GtkWidget *row=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8);
    gtk_box_append(GTK_BOX(row), gtk_label_new("Distort type:"));
    const char *types[]={"Arc","Barrel","Polar","ScaleRotateTranslate",NULL};
    GtkWidget *dd=gtk_drop_down_new_from_strings(types);
    gtk_box_append(GTK_BOX(row),dd);
    gtk_box_append(GTK_BOX(box),row);
    GtkWidget *sp; gtk_box_append(GTK_BOX(box), make_spin_row("Degree:", -360, 360, 5, 45, &sp));
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Distort Image",_args_distort_g,sp,dd,NULL,NULL);
}

GtkWidget *build_image_warp(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:",&in_e,FALSE));
    GtkWidget *sw_sp, *wa_sp, *wl_sp;
    gtk_box_append(GTK_BOX(box), make_spin_row("Swirl (°):", -360, 360, 5, 0, &sw_sp));
    gtk_box_append(GTK_BOX(box), make_spin_row("Wave amplitude:", 0, 50, 1, 0, &wa_sp));
    gtk_box_append(GTK_BOX(box), make_spin_row("Wave length:", 1, 200, 5, 60, &wl_sp));
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Warp Image",_args_warp_g,sw_sp,wa_sp,wl_sp,NULL);
}

GtkWidget *build_image_background_removal(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:",&in_e,FALSE));
    GtkWidget *color_e; gtk_box_append(GTK_BOX(box), hv_make_entry_row("Background color:", &color_e));
    gtk_entry_set_placeholder_text(GTK_ENTRY(color_e),"white");
    GtkWidget *fuzz_sp; gtk_box_append(GTK_BOX(box), make_spin_row("Fuzz %:", 1, 50, 1, 10, &fuzz_sp));
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Remove Background",_args_bgrem_g,color_e,fuzz_sp,NULL,NULL);
}

GtkWidget *build_image_portrait_retouch(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:",&in_e,FALSE));
    GtkWidget *note=gtk_label_new("Auto-sharpening, slight saturation boost, and auto-level.");
    gtk_widget_set_halign(note,GTK_ALIGN_START); gtk_box_append(GTK_BOX(box),note);
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Retouch Portrait",_args_portrait_g,NULL,NULL,NULL,NULL);
}

GtkWidget *build_image_blemish_removal(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:",&in_e,FALSE));
    GtkWidget *x_sp, *y_sp, *r_sp;
    gtk_box_append(GTK_BOX(box), make_spin_row("Blemish center X:", 0, 65535, 1, 100, &x_sp));
    gtk_box_append(GTK_BOX(box), make_spin_row("Blemish center Y:", 0, 65535, 1, 100, &y_sp));
    gtk_box_append(GTK_BOX(box), make_spin_row("Radius:", 2, 100, 1, 10, &r_sp));
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Remove Blemish",_args_blemish_g,x_sp,y_sp,r_sp,NULL);
}

GtkWidget *build_image_teeth_whitening(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:",&in_e,FALSE));
    GtkWidget *x_sp, *y_sp, *w_sp, *h_sp;
    gtk_box_append(GTK_BOX(box), make_spin_row("Teeth region X:", 0, 65535, 1, 100, &x_sp));
    gtk_box_append(GTK_BOX(box), make_spin_row("Teeth region Y:", 0, 65535, 1, 100, &y_sp));
    gtk_box_append(GTK_BOX(box), make_spin_row("Width:", 10, 1000, 5, 100, &w_sp));
    gtk_box_append(GTK_BOX(box), make_spin_row("Height:", 10, 500, 5, 40, &h_sp));
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Whiten Teeth",_args_teeth_g,x_sp,y_sp,w_sp,h_sp);
}

GtkWidget *build_image_skin_smoothing(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:",&in_e,FALSE));
    GtkWidget *sp; gtk_box_append(GTK_BOX(box), make_spin_row("Smoothing sigma:", 0.5, 10, 0.5, 2.0, &sp));
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Smooth Skin",_args_skin_g,sp,NULL,NULL,NULL);
}

GtkWidget *build_image_watermark(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:",&in_e,FALSE));
    GtkWidget *text_e; gtk_box_append(GTK_BOX(box), hv_make_entry_row("Watermark text:", &text_e));
    gtk_entry_set_placeholder_text(GTK_ENTRY(text_e),"© 2025 My Name");
    GtkWidget *alpha_sp; gtk_box_append(GTK_BOX(box), make_spin_row("Opacity (%):", 10, 100, 5, 60, &alpha_sp));
    GtkWidget *row=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8);
    gtk_box_append(GTK_BOX(row), gtk_label_new("Position:"));
    const char *gravs[]={"Bottom-Right","Bottom","Center","Top-Right","Top",NULL};
    GtkWidget *grav_dd=gtk_drop_down_new_from_strings(gravs);
    gtk_box_append(GTK_BOX(row),grav_dd);
    gtk_box_append(GTK_BOX(box),row);
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Add Watermark",_args_watermark_g,text_e,alpha_sp,grav_dd,NULL);
}

GtkWidget *build_image_border_frame(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:",&in_e,FALSE));
    GtkWidget *w_sp; gtk_box_append(GTK_BOX(box), make_spin_row("Border width (px):", 1, 200, 5, 20, &w_sp));
    GtkWidget *color_e; gtk_box_append(GTK_BOX(box), hv_make_entry_row("Border color:", &color_e));
    gtk_entry_set_placeholder_text(GTK_ENTRY(color_e),"black");
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Add Border",_args_border_g,w_sp,color_e,NULL,NULL);
}

GtkWidget *build_image_drop_shadow(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:",&in_e,FALSE));
    GtkWidget *s_sp, *xo_sp, *yo_sp;
    gtk_box_append(GTK_BOX(box), make_spin_row("Shadow blur sigma:", 1, 30, 1, 8, &s_sp));
    gtk_box_append(GTK_BOX(box), make_spin_row("X offset:", -100, 100, 2, 6, &xo_sp));
    gtk_box_append(GTK_BOX(box), make_spin_row("Y offset:", -100, 100, 2, 6, &yo_sp));
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Add Drop Shadow",_args_shadow_g,s_sp,xo_sp,yo_sp,NULL);
}

GtkWidget *build_image_reflection(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:",&in_e,FALSE));
    GtkWidget *sp; gtk_box_append(GTK_BOX(box), make_spin_row("Reflection opacity (%):", 5, 80, 5, 40, &sp));
    GtkWidget *note=gtk_label_new("Output is a vertically mirrored copy with reduced opacity.");
    gtk_widget_add_css_class(note,"helvetia-fg-muted"); gtk_widget_set_halign(note,GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box),note);
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Create Reflection",_args_reflect_g,sp,NULL,NULL,NULL);
}

GtkWidget *build_image_pixel_art_scaler(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:",&in_e,FALSE));
    GtkWidget *sp; gtk_box_append(GTK_BOX(box), make_spin_row("Scale (%):", 100, 1000, 100, 400, &sp));
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Scale Pixel Art",_args_pxart_g,sp,NULL,NULL,NULL);
}

GtkWidget *build_image_compression(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:",&in_e,FALSE));
    GtkWidget *sp; gtk_box_append(GTK_BOX(box), make_spin_row("Quality (1-100):", 1, 100, 5, 75, &sp));
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Compress Image",_args_compress_g,sp,NULL,NULL,NULL);
}

GtkWidget *build_image_lossless_optimizer(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input PNG:",&in_e,FALSE));
    GtkWidget *note=gtk_label_new("Max PNG compression level + strip metadata. Output must be .png");
    gtk_widget_add_css_class(note,"helvetia-fg-muted"); gtk_widget_set_halign(note,GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box),note);
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Optimize PNG",_args_lossless_g,NULL,NULL,NULL,NULL);
}

GtkWidget *build_image_svg_to_png(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input SVG:",&in_e,FALSE));
    GtkWidget *sp; gtk_box_append(GTK_BOX(box), make_spin_row("Density (DPI):", 72, 600, 12, 144, &sp));
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Convert SVG to PNG",_args_svg2png_g,sp,NULL,NULL,NULL);
}

GtkWidget *build_image_opacity_control(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input image:",&in_e,FALSE));
    GtkWidget *sp; gtk_box_append(GTK_BOX(box), make_spin_row("Opacity (%):", 0, 100, 5, 50, &sp));
    GtkWidget *out_e=NULL;
    return _finish_gen1(box,in_e,out_e,"Set Opacity",_args_opacity_g,sp,NULL,NULL,NULL);
}

/* --- Multi-image tools --- */

/* Grid Layout / Montage */
typedef struct { GtkWidget *folder_e, *tile_e, *geo_e, *out_e, *status; } MontageCtx;
static void on_montage(GtkButton *btn, gpointer ud) {
    (void)btn;
    MontageCtx *ctx = ud;
    const char *folder = gtk_editable_get_text(GTK_EDITABLE(ctx->folder_e));
    const char *tile   = gtk_editable_get_text(GTK_EDITABLE(ctx->tile_e));
    const char *geo    = gtk_editable_get_text(GTK_EDITABLE(ctx->geo_e));
    const char *out    = gtk_editable_get_text(GTK_EDITABLE(ctx->out_e));
    if (!folder||!*folder||!out||!*out) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Fill folder and output"); return;
    }
    if (!tile||!*tile) tile = "4x";
    if (!geo||!*geo)   geo  = "200x200+4+4";
    char *qf=g_shell_quote(folder), *qo=g_shell_quote(out);
    char *cmd = g_strdup_printf("montage %s/*.png %s/*.jpg -geometry '%s' -tile '%s' %s 2>&1",
                                qf, qf, geo, tile, qo);
    g_free(qf); g_free(qo);
    char *res = hv_run_cmd(cmd); g_free(cmd);
    gtk_label_set_text(GTK_LABEL(ctx->status),
        (!res||!*res||strstr(res,"WARNING")==res) ? "✓ Grid created!" : res);
    g_free(res);
}

GtkWidget *build_image_grid_layout(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *folder_e, *out_e, *tile_e, *geo_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Image folder:", &folder_e, FALSE));
    gtk_box_append(GTK_BOX(box), hv_make_entry_row("Tile (cols x rows):", &tile_e));
    gtk_entry_set_placeholder_text(GTK_ENTRY(tile_e), "4x3");
    gtk_box_append(GTK_BOX(box), hv_make_entry_row("Geometry (WxH+gapX+gapY):", &geo_e));
    gtk_entry_set_placeholder_text(GTK_ENTRY(geo_e), "200x200+4+4");
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output:", &out_e, TRUE));
    GtkWidget *btn = hv_make_action_btn("Create Grid");
    GtkWidget *status = hv_make_result_label();
    MontageCtx *ctx = g_new0(MontageCtx, 1);
    ctx->folder_e=folder_e; ctx->tile_e=tile_e; ctx->geo_e=geo_e; ctx->out_e=out_e; ctx->status=status;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_montage), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);
    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), status);
    return box;
}

/* Multi-input tools: panorama / HDR / focus stack */
typedef struct { GtkWidget *entries[8]; int n; GtkWidget *out_e, *status; const char *op; } MultiImgCtx;

static void on_multi_img(GtkButton *btn, gpointer ud) {
    (void)btn;
    MultiImgCtx *ctx = ud;
    const char *out = gtk_editable_get_text(GTK_EDITABLE(ctx->out_e));
    if (!out||!*out) { gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Set output"); return; }
    GString *inputs = g_string_new(NULL);
    int count = 0;
    for (int i = 0; i < ctx->n; i++) {
        const char *p = gtk_editable_get_text(GTK_EDITABLE(ctx->entries[i]));
        if (p&&*p) { char *q=g_shell_quote(p); g_string_append_printf(inputs, "%s ", q); g_free(q); count++; }
    }
    if (count < 2) { gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Add at least 2 images"); g_string_free(inputs,TRUE); return; }
    char *qo=g_shell_quote(out);
    char *cmd = g_strdup_printf("convert %s %s %s 2>&1", inputs->str, ctx->op, qo);
    g_string_free(inputs,TRUE); g_free(qo);
    char *res = hv_run_cmd(cmd); g_free(cmd);
    gtk_label_set_text(GTK_LABEL(ctx->status),
        (!res||!*res||strstr(res,"WARNING")==res) ? "✓ Done!" : res);
    g_free(res);
}

static GtkWidget *make_multi_img_tool(int n_inputs, const char *btn_label, const char *op) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    MultiImgCtx *ctx = g_new0(MultiImgCtx,1);
    ctx->n = n_inputs; ctx->op = op;
    for (int i = 0; i < n_inputs; i++) {
        char lbl[32]; snprintf(lbl,sizeof lbl,"Image %d:", i+1);
        gtk_box_append(GTK_BOX(box), hv_make_file_picker_row(lbl, &ctx->entries[i], FALSE));
    }
    GtkWidget *out_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output:", &out_e, TRUE));
    ctx->out_e = out_e;
    GtkWidget *btn = hv_make_action_btn(btn_label);
    ctx->status = hv_make_result_label();
    g_signal_connect(btn,"clicked",G_CALLBACK(on_multi_img),ctx);
    g_signal_connect_swapped(box,"destroy",G_CALLBACK(g_free),ctx);
    gtk_box_append(GTK_BOX(box),btn);
    gtk_box_append(GTK_BOX(box),ctx->status);
    return box;
}

GtkWidget *build_image_panorama_stitch(void) { return make_multi_img_tool(4,"Stitch Panorama","+append"); }
GtkWidget *build_image_hdr_merge(void)       { return make_multi_img_tool(4,"Merge HDR","-evaluate-sequence Mean"); }
GtkWidget *build_image_focus_stack(void)     { return make_multi_img_tool(4,"Stack Focus","-evaluate-sequence Max"); }
GtkWidget *build_image_image_stack(void)     { return make_multi_img_tool(4,"Flatten Stack","-flatten"); }

/* Simple IM format conversion: HEIC/RAW/PNG→SVG */
static void on_simple_convert(GtkButton *btn, gpointer ud) {
    (void)btn;
    GtkWidget *status = ud;
    GtkWidget *in_e = g_object_get_data(G_OBJECT(btn), "in-entry");
    GtkWidget *out_e = g_object_get_data(G_OBJECT(btn), "out-entry");
    const char *in = gtk_editable_get_text(GTK_EDITABLE(in_e));
    const char *out = gtk_editable_get_text(GTK_EDITABLE(out_e));
    if (!in||!*in||!out||!*out) { gtk_label_set_text(GTK_LABEL(status), "⚠ Fill both paths"); return; }
    const char *extra = g_object_get_data(G_OBJECT(btn), "extra-args");
    char *qi=g_shell_quote(in), *qo=g_shell_quote(out);
    char *cmd = g_strdup_printf("convert %s %s %s 2>&1", qi, extra ? extra : "", qo);
    g_free(qi); g_free(qo);
    char *res = hv_run_cmd(cmd); g_free(cmd);
    gtk_label_set_text(GTK_LABEL(status),
        (!res||!*res||strstr(res,"WARNING")==res) ? "✓ Converted!" : res);
    g_free(res);
}

static GtkWidget *make_simple_convert_tool(const char *in_label, const char *out_label,
                                            const char *btn_label, const char *extra_args) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e, *out_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row(in_label, &in_e, FALSE));
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row(out_label, &out_e, TRUE));
    GtkWidget *btn = hv_make_action_btn(btn_label);
    GtkWidget *status = hv_make_result_label();
    g_object_set_data(G_OBJECT(btn), "in-entry",  in_e);
    g_object_set_data(G_OBJECT(btn), "out-entry", out_e);
    g_object_set_data(G_OBJECT(btn), "extra-args", (gpointer)extra_args);
    g_signal_connect(btn,"clicked",G_CALLBACK(on_simple_convert),status);
    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), status);
    return box;
}

GtkWidget *build_image_heic_to_jpg(void) {
    return make_simple_convert_tool("Input HEIC:","Output JPG:","Convert HEIC → JPG","-quality 92");
}
typedef struct { GtkWidget *in_e, *out_e, *status; } RawCtx;

static void on_raw_convert(GtkButton *btn, gpointer ud) {
    (void)btn;
    RawCtx *ctx = ud;
    const char *in = gtk_editable_get_text(GTK_EDITABLE(ctx->in_e));
    const char *out = gtk_editable_get_text(GTK_EDITABLE(ctx->out_e));

    if (!in || !*in || !out || !*out) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Fill both paths");
        return;
    }

    int width = 0, height = 0;
    unsigned char *raw_pixels = decode_raw_file(in, &width, &height);
    if (!raw_pixels) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Failed to decode RAW file");
        return;
    }

    GeglBuffer *processed_buffer = process_with_gegl(raw_pixels, width, height);
    if (!processed_buffer) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ GEGL processing failed");
        g_free(raw_pixels);
        return;
    }

    /* Convert to GdkPixbuf and save */
    GdkPixbuf *pb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, width, height);
    if (pb) {
        gegl_buffer_get(processed_buffer, GEGL_RECTANGLE(0, 0, width, height), 1.0, babl_format("R'G'B' u8"), gdk_pixbuf_get_pixels(pb), GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
        
        GError *err = NULL;
        gboolean ok = gdk_pixbuf_save(pb, out, "jpeg", &err, "quality", "95", NULL);

        if (ok) {
            gtk_label_set_text(GTK_LABEL(ctx->status), "✓ RAW converted and processed successfully!");
        } else {
            char msg[256];
            snprintf(msg, sizeof msg, "⚠ %s", err ? err->message : "Save failed");
            gtk_label_set_text(GTK_LABEL(ctx->status), msg);
            if (err) g_error_free(err);
        }
        g_object_unref(pb);
    } else {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Failed to create GdkPixbuf");
    }

    g_object_unref(processed_buffer);
    g_free(raw_pixels);
}

GtkWidget *build_image_raw_to_jpg(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *in_e, *out_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input RAW:", &in_e, FALSE));
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output JPG:", &out_e, TRUE));
    GtkWidget *btn = hv_make_action_btn("Convert RAW → JPG");
    GtkWidget *status = hv_make_result_label();

    RawCtx *ctx = g_new0(RawCtx, 1);
    ctx->in_e = in_e;
    ctx->out_e = out_e;
    ctx->status = status;

    g_signal_connect(btn, "clicked", G_CALLBACK(on_raw_convert), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);

    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), status);
    return box;
}
GtkWidget *build_image_png_to_svg(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e, *out_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input PNG:", &in_e, FALSE));
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output SVG:", &out_e, TRUE));
    GtkWidget *note=gtk_label_new(
        "True PNG→SVG tracing requires 'potrace'. If installed, click Convert.\n"
        "Otherwise the output will be an SVG with an embedded raster bitmap.");
    gtk_label_set_wrap(GTK_LABEL(note),TRUE);
    gtk_widget_add_css_class(note,"helvetia-fg-muted");
    gtk_box_append(GTK_BOX(box),note);
    GtkWidget *btn = hv_make_action_btn("Convert PNG → SVG");
    GtkWidget *status = hv_make_result_label();
    g_object_set_data(G_OBJECT(btn), "in-entry",  in_e);
    g_object_set_data(G_OBJECT(btn), "out-entry", out_e);
    g_object_set_data(G_OBJECT(btn), "extra-args", (gpointer)"-define svg:explicit-ids=true");
    g_signal_connect(btn,"clicked",G_CALLBACK(on_simple_convert),status);
    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), status);
    return box;
}

/* Sprite Sheet Slicer */
typedef struct { GtkWidget *in_e, *tw_sp, *th_sp, *out_e, *status; } SpriteCtx;
static void on_sprite_slice(GtkButton *btn, gpointer ud) {
    (void)btn;
    SpriteCtx *ctx=ud;
    const char *in = gtk_editable_get_text(GTK_EDITABLE(ctx->in_e));
    const char *out_folder = gtk_editable_get_text(GTK_EDITABLE(ctx->out_e));
    if (!in||!*in||!out_folder||!*out_folder) {
        gtk_label_set_text(GTK_LABEL(ctx->status),"⚠ Fill paths"); return;
    }
    int tw=(int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->tw_sp));
    int th=(int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->th_sp));
    char *qi=g_shell_quote(in);
    char *cmd=g_strdup_printf("convert %s -crop %dx%d +repage '%s/sprite_%%04d.png' 2>&1",
                              qi, tw, th, out_folder);
    g_free(qi);
    char *res=hv_run_cmd(cmd); g_free(cmd);
    gtk_label_set_text(GTK_LABEL(ctx->status),
        (!res||!*res||strstr(res,"WARNING")==res) ? "✓ Sprites sliced!" : res);
    g_free(res);
}

GtkWidget *build_image_sprite_sheet_slicer(void) {
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    GtkWidget *in_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Sprite sheet:", &in_e, FALSE));
    GtkWidget *tw_sp, *th_sp;
    gtk_box_append(GTK_BOX(box), make_spin_row("Tile width (px):", 1, 2048, 1, 64, &tw_sp));
    gtk_box_append(GTK_BOX(box), make_spin_row("Tile height (px):", 1, 2048, 1, 64, &th_sp));
    GtkWidget *out_e; gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output folder:", &out_e, FALSE));
    GtkWidget *btn=hv_make_action_btn("Slice Sprite Sheet");
    GtkWidget *status=hv_make_result_label();
    SpriteCtx *ctx=g_new0(SpriteCtx,1);
    ctx->in_e=in_e; ctx->tw_sp=tw_sp; ctx->th_sp=th_sp; ctx->out_e=out_e; ctx->status=status;
    g_signal_connect(btn,"clicked",G_CALLBACK(on_sprite_slice),ctx);
    g_signal_connect_swapped(box,"destroy",G_CALLBACK(g_free),ctx);
    gtk_box_append(GTK_BOX(box),btn);
    gtk_box_append(GTK_BOX(box),status);
    return box;
}

/* Interactive canvas placeholders */
GtkWidget *build_image_brush_tool(void)       { return make_canvas_placeholder("Brush Tool"); }
GtkWidget *build_image_eraser(void)           { return make_canvas_placeholder("Eraser"); }
GtkWidget *build_image_fill_bucket(void)      { return make_canvas_placeholder("Fill / Bucket"); }
GtkWidget *build_image_gradient_tool(void)    { return make_canvas_placeholder("Gradient Tool"); }
GtkWidget *build_image_arrow_tool(void)       { return make_canvas_placeholder("Arrow Tool"); }
GtkWidget *build_image_clone_stamp(void)      { return make_canvas_placeholder("Clone Stamp"); }
GtkWidget *build_image_healing_brush(void)    { return make_canvas_placeholder("Healing Brush"); }
GtkWidget *build_image_smudge_tool(void)      { return make_canvas_placeholder("Smudge Tool"); }
GtkWidget *build_image_dodge_burn(void)       { return make_canvas_placeholder("Dodge / Burn"); }
GtkWidget *build_image_selection_tools(void)  { return make_canvas_placeholder("Selection Tools"); }
GtkWidget *build_image_layer_manager(void)    { return make_canvas_placeholder("Layer Manager"); }
GtkWidget *build_image_layer_masks(void)      { return make_canvas_placeholder("Layer Masks"); }
GtkWidget *build_image_screen_recorder(void)  { return make_canvas_placeholder("Screen Recorder"); }
