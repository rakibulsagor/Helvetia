#include <gtk/gtk.h>
#include <adwaita.h>
#include <math.h>
#include <string.h>
#include "../image_shared.h"
#include "image_levels.h"

/* ------------------------------------------------------------------ */
/* State                                                              */
/* ------------------------------------------------------------------ */

typedef struct {
    GdkPixbuf *original;
    GdkPixbuf *preview;
    char      *path;

    int        in_black;    /* 0..255 */
    int        in_white;    /* 0..255 */
    double     gamma;       /* 0.1..10.0, 1.0 = neutral */
    int        out_black;   /* 0..255 */
    int        out_white;   /* 0..255 */

    /* Histogram of the original image (256 bins, grayscale) */
    guint      histogram[256];
    guint      hist_max;

    GtkWidget *stack;
    double zoom;
    GtkWidget *picture;
    GtkWidget *hist_area;
    GtkWidget *root;

    GtkWidget *in_black_scale, *in_black_lbl;
    GtkWidget *in_white_scale, *in_white_lbl;
    GtkWidget *gamma_scale,    *gamma_lbl;
    GtkWidget *out_black_scale,*out_black_lbl;
    GtkWidget *out_white_scale,*out_white_lbl;

    GdkPixbuf *first_original;
    GPtrArray *undo_stack;
    GtkWidget *undo_btn;
} LevelsState;

static LevelsState *get_state(GtkWidget *v) {
    return g_object_get_data(G_OBJECT(v), "levels-state");
}

/* ------------------------------------------------------------------ */
/* Histogram computation                                              */
/* ------------------------------------------------------------------ */

static void compute_histogram(LevelsState *st) {
    memset(st->histogram, 0, sizeof st->histogram);
    st->hist_max = 0;

    if (!st->original) return;

    int w = gdk_pixbuf_get_width(st->original);
    int h = gdk_pixbuf_get_height(st->original);
    int n = gdk_pixbuf_get_n_channels(st->original);
    int stride = gdk_pixbuf_get_rowstride(st->original);
    guchar *px = gdk_pixbuf_get_pixels(st->original);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            guchar *p = px + y * stride + x * n;
            /* Rec. 709 luma */
            double lum = 0.2126 * p[0] + 0.7152 * p[1] + 0.0722 * p[2];
            int bin = (int)(lum + 0.5);
            if (bin < 0) bin = 0;
            if (bin > 255) bin = 255;
            st->histogram[bin]++;
        }
    }

    /* Find max ignoring the extremes (often spiky from pure black/white) */
    for (int i = 2; i < 254; i++) {
        if (st->histogram[i] > st->hist_max)
            st->hist_max = st->histogram[i];
    }
    if (st->hist_max == 0) st->hist_max = 1;
}

/* ------------------------------------------------------------------ */
/* Histogram drawing                                                  */
/* ------------------------------------------------------------------ */

static void on_hist_draw(GtkDrawingArea *area, cairo_t *cr,
                         int w, int h, gpointer data) {
    (void)area;
    LevelsState *st = data;

    /* Background */
    cairo_set_source_rgb(cr, 0.08, 0.08, 0.08);
    cairo_paint(cr);

    if (st->hist_max == 0) return;

    /* Draw bars */
    double bar_w = (double)w / 256.0;
    for (int i = 0; i < 256; i++) {
        double norm = (double)st->histogram[i] / st->hist_max;
        if (norm > 1.0) norm = 1.0;
        double bar_h = norm * (h - 4);

        /* Color: shade by region */
        if (i < st->in_black)
            cairo_set_source_rgba(cr, 0.4, 0.4, 0.4, 0.35);
        else if (i > st->in_white)
            cairo_set_source_rgba(cr, 0.4, 0.4, 0.4, 0.35);
        else
            cairo_set_source_rgb(cr, 0.85, 0.85, 0.85);

        cairo_rectangle(cr, i * bar_w, h - bar_h, bar_w + 0.5, bar_h);
        cairo_fill(cr);
    }

    /* Markers for black point, white point, gamma midpoint */
    cairo_set_line_width(cr, 2.0);

    /* Input black */
    cairo_set_source_rgb(cr, 0.2, 0.4, 0.9);
    cairo_move_to(cr, st->in_black * bar_w, 0);
    cairo_line_to(cr, st->in_black * bar_w, h);
    cairo_stroke(cr);

    /* Input white */
    cairo_set_source_rgb(cr, 0.2, 0.4, 0.9);
    cairo_move_to(cr, (st->in_white + 1) * bar_w, 0);
    cairo_line_to(cr, (st->in_white + 1) * bar_w, h);
    cairo_stroke(cr);

    /* Gamma midpoint (visual indicator) */
    double range = st->in_white - st->in_black;
    if (range > 0) {
        double midpoint = st->in_black +
                          range * pow(0.5, 1.0 / st->gamma);
        cairo_set_source_rgb(cr, 0.9, 0.7, 0.2);
        cairo_move_to(cr, midpoint * bar_w, 0);
        cairo_line_to(cr, midpoint * bar_w, h);
        cairo_stroke(cr);
    }

    /* Border */
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.15);
    cairo_set_line_width(cr, 1.0);
    cairo_rectangle(cr, 0.5, 0.5, w - 1, h - 1);
    cairo_stroke(cr);
}

/* ------------------------------------------------------------------ */
/* Preview                                                            */
/* ------------------------------------------------------------------ */

static void update_preview(LevelsState *st) {
    GdkPixbuf *src = st->preview ? st->preview : st->original;
    if (!src) return;
    gtk_picture_set_paintable(GTK_PICTURE(st->picture), GDK_PAINTABLE(gdk_texture_new_for_pixbuf(src)));
    gtk_picture_set_content_fit(GTK_PICTURE(st->picture),
                                 GTK_CONTENT_FIT_CONTAIN);
    gtk_picture_set_can_shrink(GTK_PICTURE(st->picture), TRUE);
    
}

/* ------------------------------------------------------------------ */
/* Levels formula                                                     */
/* ------------------------------------------------------------------ */

/*
 * For each pixel channel value v in [0, 255]:
 *   1. Normalize:  n = (v - in_black) / (in_white - in_black)
 *   2. Clamp:      n = clamp(n, 0, 1)
 *   3. Gamma:      n = pow(n, 1/gamma)
 *   4. Scale:      out = n * (out_white - out_black) + out_black
 *   5. Clamp:      out = clamp(out, 0, 255)
 */

static GdkPixbuf *apply_levels(LevelsState *st) {
    GdkPixbuf *src = st->original;
    int w = gdk_pixbuf_get_width(src);
    int h = gdk_pixbuf_get_height(src);
    int n = gdk_pixbuf_get_n_channels(src);
    int stride = gdk_pixbuf_get_rowstride(src);
    guchar *px = gdk_pixbuf_get_pixels(src);

    GdkPixbuf *out = gdk_pixbuf_copy(src);
    int ostride = gdk_pixbuf_get_rowstride(out);
    guchar *opx = gdk_pixbuf_get_pixels(out);

    double in_b = st->in_black;
    double in_w = st->in_white;
    double range_in = in_w - in_b;
    if (range_in < 1) range_in = 1;

    double inv_gamma = 1.0 / st->gamma;
    double out_b = st->out_black;
    double out_range = st->out_white - st->out_black;

    /* Precompute LUT for speed */
    guchar lut[256];
    for (int i = 0; i < 256; i++) {
        double v = ((double)i - in_b) / range_in;
        if (v < 0) v = 0;
        if (v > 1) v = 1;
        v = pow(v, inv_gamma);
        double o = v * out_range + out_b;
        if (o < 0) o = 0;
        if (o > 255) o = 255;
        lut[i] = (guchar)(o + 0.5);
    }

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            guchar *sp = px + y * stride + x * n;
            guchar *dp = opx + y * ostride + x * n;
            dp[0] = lut[sp[0]];
            dp[1] = lut[sp[1]];
            dp[2] = lut[sp[2]];
            if (n == 4) dp[3] = sp[3];
        }
    }
    return out;
}

static void regenerate(LevelsState *st) {
    if (!st->original) return;

    /* If everything is at defaults, just show the original */
    if (st->in_black == 0 && st->in_white == 255 &&
        fabs(st->gamma - 1.0) < 0.001 &&
        st->out_black == 0 && st->out_white == 255) {
        g_clear_object(&st->preview);
        update_preview(st);
        return;
    }

    g_clear_object(&st->preview);
    st->preview = apply_levels(st);
    update_preview(st);
}

/* ------------------------------------------------------------------ */
/* Slider callbacks                                                   */
/* ------------------------------------------------------------------ */


typedef struct {
    int    in_black, in_white;
    double gamma;
    int    out_black, out_white;
} LevelsSnapshot;

static LevelsSnapshot levels_snapshot(LevelsState *st) {
    return (LevelsSnapshot){
        .in_black = st->in_black,
        .in_white = st->in_white,
        .gamma = st->gamma,
        .out_black = st->out_black,
        .out_white = st->out_white,
    };
}

static void levels_restore(LevelsState *st, LevelsSnapshot s) {
    st->in_black = s.in_black;
    st->in_white = s.in_white;
    st->gamma = s.gamma;
    st->out_black = s.out_black;
    st->out_white = s.out_white;

    gtk_range_set_value(GTK_RANGE(st->in_black_scale), s.in_black);
    gtk_range_set_value(GTK_RANGE(st->in_white_scale), s.in_white);
    gtk_range_set_value(GTK_RANGE(st->gamma_scale), s.gamma);
    gtk_range_set_value(GTK_RANGE(st->out_black_scale), s.out_black);
    gtk_range_set_value(GTK_RANGE(st->out_white_scale), s.out_white);

    char buf[16];
    snprintf(buf, sizeof buf, "%d", s.in_black);  gtk_label_set_text(GTK_LABEL(st->in_black_lbl), buf);
    snprintf(buf, sizeof buf, "%d", s.in_white);  gtk_label_set_text(GTK_LABEL(st->in_white_lbl), buf);
    snprintf(buf, sizeof buf, "%.2f", s.gamma);   gtk_label_set_text(GTK_LABEL(st->gamma_lbl), buf);
    snprintf(buf, sizeof buf, "%d", s.out_black); gtk_label_set_text(GTK_LABEL(st->out_black_lbl), buf);
    snprintf(buf, sizeof buf, "%d", s.out_white); gtk_label_set_text(GTK_LABEL(st->out_white_lbl), buf);

    regenerate(st);
    gtk_widget_queue_draw(st->hist_area);
}

static void push_snapshot(LevelsState *st) {
    LevelsSnapshot *s = g_new0(LevelsSnapshot, 1);
    *s = levels_snapshot(st);
    g_ptr_array_add(st->undo_stack, s);
    gtk_widget_set_sensitive(st->undo_btn, TRUE);
}

static void on_in_black(GtkRange *r, gpointer d) {
    LevelsState *st = get_state(d);
    int new_val = (int)gtk_range_get_value(r);
    if (new_val == st->in_black) return;
    push_snapshot(st);
    st->in_black = new_val;

    /* Enforce black < white */
    if (st->in_black >= st->in_white) {
        st->in_black = st->in_white - 1;
        if (st->in_black < 0) st->in_black = 0;
        gtk_range_set_value(r, st->in_black);
    }

    char buf[16];
    snprintf(buf, sizeof buf, "%d", st->in_black);
    gtk_label_set_text(GTK_LABEL(st->in_black_lbl), buf);

    regenerate(st);
    gtk_widget_queue_draw(st->hist_area);
}

static void on_in_white(GtkRange *r, gpointer d) {
    LevelsState *st = get_state(d);
    int new_val = (int)gtk_range_get_value(r);
    if (new_val == st->in_white) return;
    push_snapshot(st);
    st->in_white = new_val;

    if (st->in_white <= st->in_black) {
        st->in_white = st->in_black + 1;
        if (st->in_white > 255) st->in_white = 255;
        gtk_range_set_value(r, st->in_white);
    }

    char buf[16];
    snprintf(buf, sizeof buf, "%d", st->in_white);
    gtk_label_set_text(GTK_LABEL(st->in_white_lbl), buf);

    regenerate(st);
    gtk_widget_queue_draw(st->hist_area);
}

static void on_gamma(GtkRange *r, gpointer d) {
    LevelsState *st = get_state(d);
    double new_val = gtk_range_get_value(r);
    if (fabs(new_val - st->gamma) < 0.001) return;
    push_snapshot(st);
    st->gamma = new_val;

    char buf[16];
    snprintf(buf, sizeof buf, "%.2f", st->gamma);
    gtk_label_set_text(GTK_LABEL(st->gamma_lbl), buf);

    regenerate(st);
    gtk_widget_queue_draw(st->hist_area);
}

static void on_out_black(GtkRange *r, gpointer d) {
    LevelsState *st = get_state(d);
    int new_val = (int)gtk_range_get_value(r);
    if (new_val == st->out_black) return;
    push_snapshot(st);
    st->out_black = new_val;

    if (st->out_black >= st->out_white) {
        st->out_black = st->out_white - 1;
        if (st->out_black < 0) st->out_black = 0;
        gtk_range_set_value(r, st->out_black);
    }

    char buf[16];
    snprintf(buf, sizeof buf, "%d", st->out_black);
    gtk_label_set_text(GTK_LABEL(st->out_black_lbl), buf);

    regenerate(st);
}

static void on_out_white(GtkRange *r, gpointer d) {
    LevelsState *st = get_state(d);
    int new_val = (int)gtk_range_get_value(r);
    if (new_val == st->out_white) return;
    push_snapshot(st);
    st->out_white = new_val;

    if (st->out_white <= st->out_black) {
        st->out_white = st->out_black + 1;
        if (st->out_white > 255) st->out_white = 255;
        gtk_range_set_value(r, st->out_white);
    }

    char buf[16];
    snprintf(buf, sizeof buf, "%d", st->out_white);
    gtk_label_set_text(GTK_LABEL(st->out_white_lbl), buf);

    regenerate(st);
}

/* ------------------------------------------------------------------ */
/* Reset                                                              */
/* ------------------------------------------------------------------ */

static void reset_all(LevelsState *st) {
    st->in_black = 0;
    st->in_white = 255;
    st->gamma = 1.0;
    st->out_black = 0;
    st->out_white = 255;

    gtk_range_set_value(GTK_RANGE(st->in_black_scale), 0);
    gtk_range_set_value(GTK_RANGE(st->in_white_scale), 255);
    gtk_range_set_value(GTK_RANGE(st->gamma_scale), 1.0);
    gtk_range_set_value(GTK_RANGE(st->out_black_scale), 0);
    gtk_range_set_value(GTK_RANGE(st->out_white_scale), 255);

    gtk_label_set_text(GTK_LABEL(st->in_black_lbl), "0");
    gtk_label_set_text(GTK_LABEL(st->in_white_lbl), "255");
    gtk_label_set_text(GTK_LABEL(st->gamma_lbl), "1.00");
    gtk_label_set_text(GTK_LABEL(st->out_black_lbl), "0");
    gtk_label_set_text(GTK_LABEL(st->out_white_lbl), "255");

    regenerate(st);
    gtk_widget_queue_draw(st->hist_area);
}

static void on_undo(GtkButton *b, gpointer d) {
    (void)b;
    LevelsState *st = get_state(d);
    if (st->undo_stack->len == 0) return;

    LevelsSnapshot *s = g_ptr_array_index(st->undo_stack,
                                            st->undo_stack->len - 1);
    g_ptr_array_remove_index(st->undo_stack, st->undo_stack->len - 1);

    levels_restore(st, *s);
    g_free(s);

    gtk_widget_set_sensitive(st->undo_btn, st->undo_stack->len > 0);
}

static void on_reset(GtkButton *b, gpointer d) {
    (void)b;
    LevelsState *st = get_state(d);
    if (!st->original) return;
    push_snapshot(st);
    reset_all(st);
}

/* ------------------------------------------------------------------ */
/* Save                                                               */
/* ------------------------------------------------------------------ */

static void on_save(GtkButton *b, gpointer d) {
    (void)b;
    LevelsState *st = get_state(d);
    GdkPixbuf *src = st->preview ? st->preview : st->original;
    if (!src) return;

    char *base = st->path ? g_path_get_basename(st->path) : g_strdup("image.png");
    char *name = g_strdup_printf("levels_%s", base);
    image_save_pixbuf_dialog(st->root, src, name);
    g_free(name);
    g_free(base);
}

/* ------------------------------------------------------------------ */
/* Load                                                               */
/* ------------------------------------------------------------------ */

static void on_drop(const char *path, gpointer d) {
    LevelsState *st = get_state(d);

    GError *e = NULL;
    GdkPixbuf *pb = gdk_pixbuf_new_from_file(path, &e);
    if (!pb) {
        image_show_error(st->root, e->message);
        g_error_free(e);
        return;
    }

    g_clear_object(&st->original);
    g_clear_object(&st->preview);
    g_free(st->path);

    g_clear_object(&st->first_original);
    st->first_original = gdk_pixbuf_copy(pb);
    if (st->undo_stack) {
        for (guint i = 0; i < st->undo_stack->len; i++)
            g_free(g_ptr_array_index(st->undo_stack, i));
        g_ptr_array_set_size(st->undo_stack, 0);
    }
    if (st->undo_btn) gtk_widget_set_sensitive(st->undo_btn, FALSE);

    st->original = pb;
    st->path = g_strdup(path);
    st->in_black = 0;
    st->in_white = 255;
    st->gamma = 1.0;
    st->out_black = 0;
    st->out_white = 255;

    compute_histogram(st);
    reset_all(st);
    update_preview(st);

    gtk_widget_queue_draw(st->hist_area);
    gtk_stack_set_visible_child_name(GTK_STACK(st->stack), "editor");
}

/* ------------------------------------------------------------------ */
/* Lifecycle                                                          */
/* ------------------------------------------------------------------ */

static void levels_state_free(LevelsState *st) {
    if (!st) return;
    g_clear_object(&st->original);
    g_clear_object(&st->preview);
    g_clear_object(&st->first_original);
    if (st->undo_stack) {
        for (guint i = 0; i < st->undo_stack->len; i++)
            g_free(g_ptr_array_index(st->undo_stack, i));
        g_ptr_array_unref(st->undo_stack);
    }
    g_free(st->path);
    g_free(st);
}

void image_levels_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "levels-state", NULL);
}

/* ------------------------------------------------------------------ */
/* Commands                                                           */
/* ------------------------------------------------------------------ */

static void cmd_reset(GtkWidget *v) {
    reset_all(get_state(v));
}

const HelvetiaToolCommand image_levels_commands[] = {
    { .id = "reset", .name = "Reset",
      .icon_name = "view-refresh-symbolic",
      .accel = NULL, .tooltip = "Reset all levels",
      .activate = cmd_reset },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

/* ------------------------------------------------------------------ */
/* Helper: label + scale + value                                   */
/* ------------------------------------------------------------------ */

static GtkWidget *build_slider_row(const char *label,
                                    double min, double max, double step,
                                    double initial,
                                    GtkWidget **out_scale,
                                    GtkWidget **out_value_lbl,
                                    GCallback changed_cb,
                                    gpointer user_data) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

    GtkWidget *lbl = gtk_label_new(label);
    gtk_widget_add_css_class(lbl, "dim-label");
    gtk_widget_set_size_request(lbl, 70, -1);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);

    GtkWidget *scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,
                                                 min, max, step);
    gtk_widget_set_size_request(scale, 180, -1);
    gtk_range_set_value(GTK_RANGE(scale), initial);
    gtk_scale_set_draw_value(GTK_SCALE(scale), FALSE);
    gtk_widget_set_hexpand(scale, TRUE);

    GtkWidget *val = gtk_label_new("");
    gtk_widget_set_size_request(val, 48, -1);
    gtk_label_set_xalign(GTK_LABEL(val), 1.0f);
    gtk_widget_add_css_class(val, "dim-label");

    gtk_box_append(GTK_BOX(row), lbl);
    gtk_box_append(GTK_BOX(row), scale);
    gtk_box_append(GTK_BOX(row), val);

    *out_scale = scale;
    *out_value_lbl = val;

    if (changed_cb)
        g_signal_connect(scale, "value-changed", changed_cb, user_data);

    return row;
}

/* ------------------------------------------------------------------ */
/* Create                                                             */
/* ------------------------------------------------------------------ */

GtkWidget *image_levels_create(void) {
    LevelsState *st = g_new0(LevelsState, 1);
    st->undo_stack = g_ptr_array_new();
    st->zoom = 1.0;
    st->in_white = 255;
    st->out_white = 255;
    st->gamma = 1.0;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;
    image_install_edit_shortcuts(root,
        (ImageToolCallback)on_undo,
        (ImageToolCallback)on_reset,
        root);

    GtkWidget *stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(stack),
                                  GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_widget_set_vexpand(stack, TRUE);
    st->stack = stack;

    /* ---- Drop page ---- */
    GtkWidget *drop_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(drop_box, 24);
    gtk_widget_set_margin_end(drop_box, 24);
    gtk_widget_set_margin_top(drop_box, 24);
    gtk_widget_set_margin_bottom(drop_box, 24);
    gtk_widget_set_vexpand(drop_box, TRUE);
    gtk_box_append(GTK_BOX(drop_box),
        image_build_drop_zone("Image file", on_drop, root));
    gtk_stack_add_named(GTK_STACK(stack), drop_box, "drop");

    /* ---- Editor page ---- */
    GtkWidget *editor = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    /* Histogram */
    GtkWidget *hist = gtk_drawing_area_new();
    gtk_widget_set_size_request(hist, -1, 100);
    gtk_widget_set_margin_start(hist, 12);
    gtk_widget_set_margin_end(hist, 12);
    gtk_widget_set_margin_top(hist, 8);
    gtk_widget_set_margin_bottom(hist, 4);
    st->hist_area = hist;
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(hist),
                                    on_hist_draw, st, NULL);

    /* Sliders */
    GtkWidget *sliders = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start(sliders, 12);
    gtk_widget_set_margin_end(sliders, 12);
    gtk_widget_set_margin_top(sliders, 4);
    gtk_widget_set_margin_bottom(sliders, 8);

    /* Input black / white */
    GtkWidget *input_lbl = gtk_label_new("Input");
    gtk_widget_add_css_class(input_lbl, "heading");
    gtk_label_set_xalign(GTK_LABEL(input_lbl), 0.0f);
    gtk_box_append(GTK_BOX(sliders), input_lbl);

    gtk_box_append(GTK_BOX(sliders),
        build_slider_row("Black", 0, 255, 1, 0,
                         &st->in_black_scale, &st->in_black_lbl,
                         G_CALLBACK(on_in_black), root));

    gtk_box_append(GTK_BOX(sliders),
        build_slider_row("Gamma", 0.1, 5.0, 0.01, 1.0,
                         &st->gamma_scale, &st->gamma_lbl,
                         G_CALLBACK(on_gamma), root));

    gtk_box_append(GTK_BOX(sliders),
        build_slider_row("White", 0, 255, 1, 255,
                         &st->in_white_scale, &st->in_white_lbl,
                         G_CALLBACK(on_in_white), root));

    /* Spacer */
    GtkWidget *sp = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(sp, -1, 8);
    gtk_box_append(GTK_BOX(sliders), sp);

    /* Output black / white */
    GtkWidget *output_lbl = gtk_label_new("Output");
    gtk_widget_add_css_class(output_lbl, "heading");
    gtk_label_set_xalign(GTK_LABEL(output_lbl), 0.0f);
    gtk_box_append(GTK_BOX(sliders), output_lbl);

    gtk_box_append(GTK_BOX(sliders),
        build_slider_row("Black", 0, 255, 1, 0,
                         &st->out_black_scale, &st->out_black_lbl,
                         G_CALLBACK(on_out_black), root));

    gtk_box_append(GTK_BOX(sliders),
        build_slider_row("White", 0, 255, 1, 255,
                         &st->out_white_scale, &st->out_white_lbl,
                         G_CALLBACK(on_out_white), root));

    /* Buttons */
    GtkWidget *btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_margin_start(btn_row, 12);
    gtk_widget_set_margin_end(btn_row, 12);
    gtk_widget_set_margin_bottom(btn_row, 8);

    GtkWidget *btn_sp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(btn_sp, TRUE);

    st->undo_btn = image_undo_button(G_CALLBACK(on_undo), root);
    gtk_widget_set_sensitive(st->undo_btn, FALSE);

    GtkWidget *reset_btn = image_reset_button(G_CALLBACK(on_reset), root);

    GtkWidget *save = gtk_button_new_with_label("Save As…");
    gtk_widget_add_css_class(save, "flat");

    gtk_box_append(GTK_BOX(btn_row), btn_sp);
    gtk_box_append(GTK_BOX(btn_row), image_new_image_button(on_drop, root));
    gtk_box_append(GTK_BOX(btn_row), st->undo_btn);
    gtk_box_append(GTK_BOX(btn_row), reset_btn);
    gtk_box_append(GTK_BOX(btn_row), save);

    /* Picture */
    GtkWidget *pic = gtk_picture_new();
    gtk_widget_set_vexpand(pic, TRUE);
    gtk_widget_set_halign(pic, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(pic, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class(pic, "image-viewer-canvas");
    st->picture = pic;

    /* Compose editor */
    gtk_box_append(GTK_BOX(editor), hist);
    gtk_box_append(GTK_BOX(editor),
                   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(editor), sliders);
    gtk_box_append(GTK_BOX(editor), btn_row);
    gtk_box_append(GTK_BOX(editor),
                   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(editor), pic);

    gtk_stack_add_named(GTK_STACK(stack), editor, "editor");
    gtk_stack_set_visible_child_name(GTK_STACK(stack), "drop");
    gtk_box_append(GTK_BOX(root), stack);

    g_object_set_data_full(G_OBJECT(root), "levels-state", st, (GDestroyNotify)levels_state_free);
    image_register_zoom(root, st->picture, &st->zoom);
    image_install_zoom_shortcuts(root);

    
    g_signal_connect(save,  "clicked", G_CALLBACK(on_save),  root);

    return root;
}
