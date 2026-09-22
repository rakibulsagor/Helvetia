#include <gtk/gtk.h>
#include <adwaita.h>
#include <math.h>
#include "../image_shared.h"
#include "image_posterize.h"

/* ================================================================== */
/* SHARED STATE                                                       */
/* ================================================================== */

typedef struct {
    GdkPixbuf *original;
    GdkPixbuf *preview;
    GdkPixbuf *first_original;
    GPtrArray *undo_stack;
    char      *path;

    /* Posterize */
    int        post_levels;     /* 2..32 */

    /* Threshold */
    int        thresh_level;    /* 0..255 */
    int        thresh_method;   /* 0=binary, 1=adaptive-ish */
    gboolean   thresh_color;    /* TRUE = per-channel, FALSE = luma */

    /* Vignette */
    double     vig_amount;      /* 0..100 */
    double     vig_radius;      /* 0..100 */
    double     vig_feather;     /* 0..100 */
    int        vig_color;       /* 0=black, 1=white */

    double     zoom;
    GtkWidget *stack, *picture, *root;
    GtkWidget *undo_btn;
} PosterizeState;

static PosterizeState *get_state(GtkWidget *v) {
    return g_object_get_data(G_OBJECT(v), "posterize-state");
}

/* ================================================================== */
/* UNDO                                                               */
/* ================================================================== */

static void push_snapshot(PosterizeState *st) {
    double *s = g_new0(double, 8);
    s[0] = st->post_levels;
    s[1] = st->thresh_level;
    s[2] = st->thresh_method;
    s[3] = st->thresh_color ? 1 : 0;
    s[4] = st->vig_amount;
    s[5] = st->vig_radius;
    s[6] = st->vig_feather;
    s[7] = st->vig_color;
    g_ptr_array_add(st->undo_stack, s);
    gtk_widget_set_sensitive(st->undo_btn, TRUE);
}

static void on_undo(GtkButton *b, gpointer d) {
    (void)b;
    PosterizeState *st = get_state(d);
    if (st->undo_stack->len == 0) return;
    double *s = g_ptr_array_index(st->undo_stack, st->undo_stack->len - 1);
    g_ptr_array_remove_index(st->undo_stack, st->undo_stack->len - 1);
    st->post_levels = (int)s[0];
    st->thresh_level = (int)s[1];
    st->thresh_method = (int)s[2];
    st->thresh_color = s[3] > 0.5;
    st->vig_amount = s[4];
    st->vig_radius = s[5];
    st->vig_feather = s[6];
    st->vig_color = (int)s[7];
    g_free(s);
    gtk_widget_set_sensitive(st->undo_btn, st->undo_stack->len > 0);
}

static void clear_undo(PosterizeState *st) {
    for (guint i = 0; i < st->undo_stack->len; i++)
        g_free(g_ptr_array_index(st->undo_stack, i));
    g_ptr_array_set_size(st->undo_stack, 0);
    gtk_widget_set_sensitive(st->undo_btn, FALSE);
}

/* ================================================================== */
/* SHARED HELPERS                                                     */
/* ================================================================== */

static void update_preview(PosterizeState *st) {
    GdkPixbuf *src = st->preview ? st->preview : st->original;
    if (!src) return;
    gtk_picture_set_paintable(GTK_PICTURE(st->picture), GDK_PAINTABLE(gdk_texture_new_for_pixbuf(src)));
    gtk_picture_set_content_fit(GTK_PICTURE(st->picture),
                                 GTK_CONTENT_FIT_CONTAIN);
    gtk_picture_set_can_shrink(GTK_PICTURE(st->picture), TRUE);
    
}

static void on_save_common(PosterizeState *st, const char *prefix) {
    GdkPixbuf *src = st->preview ? st->preview : st->original;
    if (!src) return;
    char *base = st->path ? g_path_get_basename(st->path) : g_strdup("image.png");
    char *name = g_strdup_printf("%s_%s", prefix, base);
    image_save_pixbuf_dialog(st->root, src, name);
    g_free(name);
    g_free(base);
}

static void on_drop_common(PosterizeState *st, const char *path) {
    st->zoom = 1.0;
    GError *e = NULL;
    GdkPixbuf *pb = gdk_pixbuf_new_from_file(path, &e);
    if (!pb) { image_show_error(st->root, e->message); g_error_free(e); return; }

    g_clear_object(&st->original);
    g_clear_object(&st->preview);
    g_clear_object(&st->first_original);
    g_free(st->path);
    clear_undo(st);

    st->original = pb;
    st->first_original = gdk_pixbuf_copy(pb);
    st->path = g_strdup(path);

    st->post_levels = 6;
    st->thresh_level = 128;
    st->thresh_method = 0;
    st->thresh_color = FALSE;
    st->vig_amount = 60;
    st->vig_radius = 60;
    st->vig_feather = 50;
    st->vig_color = 0;

    update_preview(st);
    gtk_stack_set_visible_child_name(GTK_STACK(st->stack), "editor");
}

static void posterize_state_free(PosterizeState *st) {
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

static inline double clampd(double v, double lo, double hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/* ================================================================== */
/* POSTERIZE                                                          */
/* ================================================================== */

/*
 * Posterize: reduce the number of distinct levels per channel.
 *   levels = N  →  quantize each channel to N equally-spaced values.
 *   out = round(v / 255 * (N-1)) / (N-1) * 255
 */

static GdkPixbuf *apply_posterize(PosterizeState *st) {
    GdkPixbuf *src = st->original;
    int w = gdk_pixbuf_get_width(src);
    int h = gdk_pixbuf_get_height(src);
    int n = gdk_pixbuf_get_n_channels(src);
    int stride = gdk_pixbuf_get_rowstride(src);
    guchar *px = gdk_pixbuf_get_pixels(src);

    int levels = st->post_levels;
    if (levels < 2) levels = 2;
    if (levels > 32) levels = 32;

    /* Build LUT for speed */
    guchar lut[256];
    double step = 255.0 / (levels - 1);
    for (int i = 0; i < 256; i++) {
        int bin = (int)((i / 255.0) * (levels - 1) + 0.5);
        if (bin > levels - 1) bin = levels - 1;
        lut[i] = (guchar)(bin * step + 0.5);
    }

    GdkPixbuf *out = gdk_pixbuf_copy(src);
    int ostride = gdk_pixbuf_get_rowstride(out);
    guchar *opx = gdk_pixbuf_get_pixels(out);

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

/* ================================================================== */
/* THRESHOLD                                                          */
/* ================================================================== */

static GdkPixbuf *apply_threshold(PosterizeState *st) {
    GdkPixbuf *src = st->original;
    int w = gdk_pixbuf_get_width(src);
    int h = gdk_pixbuf_get_height(src);
    int n = gdk_pixbuf_get_n_channels(src);
    int stride = gdk_pixbuf_get_rowstride(src);
    guchar *px = gdk_pixbuf_get_pixels(src);

    int t = st->thresh_level;

    GdkPixbuf *out = gdk_pixbuf_copy(src);
    int ostride = gdk_pixbuf_get_rowstride(out);
    guchar *opx = gdk_pixbuf_get_pixels(out);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            guchar *sp = px + y * stride + x * n;
            guchar *dp = opx + y * ostride + x * n;

            if (st->thresh_color) {
                /* Per-channel threshold */
                dp[0] = sp[0] > t ? 255 : 0;
                dp[1] = sp[1] > t ? 255 : 0;
                dp[2] = sp[2] > t ? 255 : 0;
            } else {
                /* Luma-based threshold — pure B&W output */
                double Y = 0.2126 * sp[0] + 0.7152 * sp[1] + 0.0722 * sp[2];
                guchar v = (Y > t) ? 255 : 0;
                dp[0] = dp[1] = dp[2] = v;
            }
            if (n == 4) dp[3] = sp[3];
        }
    }
    return out;
}

/* ================================================================== */
/* VIGNETTE                                                           */
/* ================================================================== */

static GdkPixbuf *apply_vignette(PosterizeState *st) {
    GdkPixbuf *src = st->original;
    int w = gdk_pixbuf_get_width(src);
    int h = gdk_pixbuf_get_height(src);
    int n = gdk_pixbuf_get_n_channels(src);
    int stride = gdk_pixbuf_get_rowstride(src);
    guchar *px = gdk_pixbuf_get_pixels(src);

    double amount = st->vig_amount / 100.0;      /* 0..1 strength */
    double radius = st->vig_radius / 100.0;      /* 0..1 where falloff starts */
    double feather = st->vig_feather / 100.0;    /* 0..1 how soft the edge is */
    if (feather < 0.01) feather = 0.01;

    /* Vignette color: 0 = black (darken), 1 = white (lighten) */
    double vig_r = (st->vig_color == 1) ? 1.0 : 0.0;
    double vig_g = vig_r;
    double vig_b = vig_r;

    double cx = w / 2.0;
    double cy = h / 2.0;
    /* Normalize distance: 1.0 at the corners */
    double max_dist = sqrt(cx * cx + cy * cy);

    GdkPixbuf *out = gdk_pixbuf_copy(src);
    int ostride = gdk_pixbuf_get_rowstride(out);
    guchar *opx = gdk_pixbuf_get_pixels(out);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            guchar *sp = px + y * stride + x * n;
            guchar *dp = opx + y * ostride + x * n;

            /* Normalized radial distance [0..1] */
            double dx = (x - cx) / max_dist;
            double dy = (y - cy) / max_dist;
            double dist = sqrt(dx * dx + dy * dy);

            /* Falloff: 0 inside radius, ramps to 1 at edge */
            double falloff = (dist - radius) / feather;
            if (falloff < 0) falloff = 0;
            if (falloff > 1) falloff = 1;
            /* Smoothstep */
            falloff = falloff * falloff * (3.0 - 2.0 * falloff);

            /* Mix pixel toward vignette color */
            double f = falloff * amount;
            double r = sp[0] / 255.0;
            double g = sp[1] / 255.0;
            double b = sp[2] / 255.0;

            r = r + (vig_r - r) * f;
            g = g + (vig_g - g) * f;
            b = b + (vig_b - b) * f;

            dp[0] = (guchar)clampd(r * 255.0 + 0.5, 0, 255);
            dp[1] = (guchar)clampd(g * 255.0 + 0.5, 0, 255);
            dp[2] = (guchar)clampd(b * 255.0 + 0.5, 0, 255);
            if (n == 4) dp[3] = sp[3];
        }
    }
    return out;
}

/* ================================================================== */
/* UI HELPERS                                                         */
/* ================================================================== */

static GtkWidget *make_slider(const char *label, double min, double max,
                                double step, double initial,
                                GtkWidget **out_scale, GtkWidget **out_lbl,
                                GCallback cb, gpointer user_data) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *lbl = gtk_label_new(label);
    gtk_widget_add_css_class(lbl, "dim-label");
    gtk_widget_set_size_request(lbl, 130, -1);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);

    GtkWidget *scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,
                                                 min, max, step);
    gtk_widget_set_size_request(scale, 220, -1);
    gtk_range_set_value(GTK_RANGE(scale), initial);
    gtk_scale_set_draw_value(GTK_SCALE(scale), FALSE);
    gtk_widget_set_hexpand(scale, TRUE);

    GtkWidget *val = gtk_label_new("");
    gtk_widget_set_size_request(val, 60, -1);
    gtk_label_set_xalign(GTK_LABEL(val), 1.0f);
    gtk_widget_add_css_class(val, "dim-label");

    gtk_box_append(GTK_BOX(row), lbl);
    gtk_box_append(GTK_BOX(row), scale);
    gtk_box_append(GTK_BOX(row), val);

    *out_scale = scale;
    *out_lbl = val;
    if (cb) g_signal_connect(scale, "value-changed", cb, user_data);
    return row;
}

static GtkWidget *make_switch_row(const char *label, gboolean initial,
                                    GCallback cb, gpointer user_data) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *lbl = gtk_label_new(label);
    gtk_widget_set_size_request(lbl, 200, -1);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
    gtk_widget_set_hexpand(lbl, TRUE);

    GtkWidget *sw = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(sw), initial);
    gtk_widget_set_valign(sw, GTK_ALIGN_CENTER);
    if (cb) g_signal_connect(sw, "notify::active", cb, user_data);

    gtk_box_append(GTK_BOX(row), lbl);
    gtk_box_append(GTK_BOX(row), sw);
    return row;
}

static GtkWidget *build_shell(PosterizeState *st, GtkWidget **out_sliders,
                                const char *drop_hint,
                                ImageDropCallback on_drop,
                                GCallback on_save,
                                GCallback on_reset,
                                gpointer root) {
    GtkWidget *stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(stack),
                                  GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_widget_set_vexpand(stack, TRUE);
    st->stack = stack;

    GtkWidget *drop_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(drop_box, 24);
    gtk_widget_set_margin_end(drop_box, 24);
    gtk_widget_set_margin_top(drop_box, 24);
    gtk_widget_set_margin_bottom(drop_box, 24);
    gtk_widget_set_vexpand(drop_box, TRUE);
    gtk_box_append(GTK_BOX(drop_box),
        image_build_drop_zone(drop_hint, on_drop, root));
    gtk_stack_add_named(GTK_STACK(stack), drop_box, "drop");

    GtkWidget *editor = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    GtkWidget *sliders = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start(sliders, 12);
    gtk_widget_set_margin_end(sliders, 12);
    gtk_widget_set_margin_top(sliders, 8);
    gtk_widget_set_margin_bottom(sliders, 8);
    *out_sliders = sliders;

    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(bar, 12);
    gtk_widget_set_margin_end(bar, 12);
    gtk_widget_set_margin_bottom(bar, 8);

    st->undo_btn = image_undo_button(G_CALLBACK(on_undo), root);
    gtk_widget_set_sensitive(st->undo_btn, FALSE);

    GtkWidget *reset = image_reset_button(on_reset, root);
    GtkWidget *new_img = image_new_image_button(on_drop, root);

    GtkWidget *sp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(sp, TRUE);

    GtkWidget *save = gtk_button_new_with_label("Save As…");
    gtk_widget_add_css_class(save, "flat");
    if (on_save) g_signal_connect(save, "clicked", on_save, root);

    gtk_box_append(GTK_BOX(bar), new_img);
    gtk_box_append(GTK_BOX(bar), gtk_separator_new(GTK_ORIENTATION_VERTICAL));
    gtk_box_append(GTK_BOX(bar), st->undo_btn);
    gtk_box_append(GTK_BOX(bar), reset);

    gtk_box_append(GTK_BOX(bar), gtk_separator_new(GTK_ORIENTATION_VERTICAL));
    GtkWidget *z_out = gtk_button_new_from_icon_name("zoom-out-symbolic");
    GtkWidget *z_in = gtk_button_new_from_icon_name("zoom-in-symbolic");
    GtkWidget *z_1 = gtk_button_new_from_icon_name("zoom-original-symbolic");
    gtk_widget_add_css_class(z_out, "flat");
    gtk_widget_add_css_class(z_in, "flat");
    gtk_widget_add_css_class(z_1, "flat");
    g_signal_connect_swapped(z_out, "clicked", G_CALLBACK(image_zoom_out), root);
    g_signal_connect_swapped(z_in, "clicked", G_CALLBACK(image_zoom_in), root);
    g_signal_connect_swapped(z_1, "clicked", G_CALLBACK(image_zoom_reset), root);
    gtk_box_append(GTK_BOX(bar), z_out);
    gtk_box_append(GTK_BOX(bar), z_1);
    gtk_box_append(GTK_BOX(bar), z_in);

    gtk_box_append(GTK_BOX(bar), sp);
    gtk_box_append(GTK_BOX(bar), save);

    GtkWidget *pic = gtk_picture_new();
    gtk_widget_set_vexpand(pic, TRUE);
    gtk_widget_set_halign(pic, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(pic, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class(pic, "image-viewer-canvas");
    st->picture = pic;

    gtk_box_append(GTK_BOX(editor), sliders);
    gtk_box_append(GTK_BOX(editor), bar);
    gtk_box_append(GTK_BOX(editor),
                   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(editor), pic);
    gtk_stack_add_named(GTK_STACK(stack), editor, "editor");
    gtk_stack_set_visible_child_name(GTK_STACK(stack), "drop");

    return stack;
}

/* ================================================================== */
/* TOOL 1 — POSTERIZE                                                 */
/* ================================================================== */

static void po_refresh(PosterizeState *st) {
    g_clear_object(&st->preview);
    st->preview = apply_posterize(st);
    update_preview(st);
}

static void po_on_levels(GtkRange *r, gpointer d) {
    PosterizeState *st = get_state(d);
    int v = (int)gtk_range_get_value(r);
    if (v == st->post_levels) return;
    push_snapshot(st);
    st->post_levels = v;
    po_refresh(st);
}

static void po_on_save(GtkButton *b, gpointer d) {
    (void)b; on_save_common(get_state(d), "posterized");
}
static void po_on_drop(const char *path, gpointer d) {
    PosterizeState *st = get_state(d);
    on_drop_common(st, path);
    if (st->original) po_refresh(st);
}
static void po_on_reset(GtkButton *b, gpointer d) {
    (void)b;
    PosterizeState *st = get_state(d);
    push_snapshot(st);
    st->post_levels = 6;
    po_refresh(st);
}

void image_posterize_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "posterize-state", NULL);
}
static void cmd_po_reset(GtkWidget *v) { po_on_reset(NULL, v); }

const HelvetiaToolCommand image_posterize_commands[] = {
    { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic",
      .accel = NULL, .tooltip = "Reset", .activate = cmd_po_reset },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

GtkWidget *image_posterize_create(void) {
    PosterizeState *st = g_new0(PosterizeState, 1);
    st->undo_stack = g_ptr_array_new();
    st->zoom = 1.0;
    st->post_levels = 6;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;

    GtkWidget *sliders;
    GtkWidget *stack = build_shell(st, &sliders, "Image file",
                                    po_on_drop, G_CALLBACK(po_on_save),
                                    G_CALLBACK(po_on_reset), root);

    GtkWidget *s, *l;
    gtk_box_append(GTK_BOX(sliders),
        make_slider("Levels", 2, 32, 1, 6, &s, &l,
                     G_CALLBACK(po_on_levels), root));
    gtk_label_set_text(GTK_LABEL(l), "6");

    GtkWidget *hint = gtk_label_new(
        "Reduces the number of distinct color levels per channel. "
        "Low values produce a flat graphic look; high values approach the original.");
    gtk_label_set_wrap(GTK_LABEL(hint), TRUE);
    gtk_widget_add_css_class(hint, "dim-label");
    gtk_widget_add_css_class(hint, "caption");
    gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);
    gtk_box_append(GTK_BOX(sliders), hint);

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "posterize-state", st,
                           (GDestroyNotify)posterize_state_free);
    image_register_zoom(root, st->picture, &st->zoom);
    image_install_zoom_shortcuts(root);
    return root;
}

/* ================================================================== */
/* TOOL 2 — THRESHOLD                                                 */
/* ================================================================== */

static void th_refresh(PosterizeState *st) {
    g_clear_object(&st->preview);
    st->preview = apply_threshold(st);
    update_preview(st);
}

static void th_on_level(GtkRange *r, gpointer d) {
    PosterizeState *st = get_state(d);
    int v = (int)gtk_range_get_value(r);
    if (v == st->thresh_level) return;
    push_snapshot(st);
    st->thresh_level = v;
    th_refresh(st);
}

static void th_on_color(GObject *sw, GParamSpec *p, gpointer d) {
    (void)p;
    PosterizeState *st = get_state(d);
    st->thresh_color = gtk_switch_get_active(GTK_SWITCH(sw));
    th_refresh(st);
}

static void th_on_save(GtkButton *b, gpointer d) {
    (void)b; on_save_common(get_state(d), "threshold");
}
static void th_on_drop(const char *path, gpointer d) {
    PosterizeState *st = get_state(d);
    on_drop_common(st, path);
    if (st->original) th_refresh(st);
}
static void th_on_reset(GtkButton *b, gpointer d) {
    (void)b;
    PosterizeState *st = get_state(d);
    push_snapshot(st);
    st->thresh_level = 128;
    st->thresh_color = FALSE;
    th_refresh(st);
}

void image_threshold_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "posterize-state", NULL);
}
static void cmd_th_reset(GtkWidget *v) { th_on_reset(NULL, v); }

const HelvetiaToolCommand image_threshold_commands[] = {
    { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic",
      .accel = NULL, .tooltip = "Reset", .activate = cmd_th_reset },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

GtkWidget *image_threshold_create(void) {
    PosterizeState *st = g_new0(PosterizeState, 1);
    st->undo_stack = g_ptr_array_new();
    st->zoom = 1.0;
    st->thresh_level = 128;
    st->thresh_color = FALSE;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;

    GtkWidget *sliders;
    GtkWidget *stack = build_shell(st, &sliders, "Image file",
                                    th_on_drop, G_CALLBACK(th_on_save),
                                    G_CALLBACK(th_on_reset), root);

    GtkWidget *s, *l;
    gtk_box_append(GTK_BOX(sliders),
        make_slider("Threshold", 0, 255, 1, 128, &s, &l,
                     G_CALLBACK(th_on_level), root));
    gtk_label_set_text(GTK_LABEL(l), "128");

    gtk_box_append(GTK_BOX(sliders),
        gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

    gtk_box_append(GTK_BOX(sliders),
        make_switch_row("Per-channel (color threshold)", FALSE,
                         G_CALLBACK(th_on_color), root));

    GtkWidget *hint = gtk_label_new(
        "Off — produces pure black-and-white based on luma. "
        "On — thresholds each RGB channel independently, keeping some color.");
    gtk_label_set_wrap(GTK_LABEL(hint), TRUE);
    gtk_widget_add_css_class(hint, "dim-label");
    gtk_widget_add_css_class(hint, "caption");
    gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);
    gtk_box_append(GTK_BOX(sliders), hint);

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "posterize-state", st,
                           (GDestroyNotify)posterize_state_free);
    image_register_zoom(root, st->picture, &st->zoom);
    image_install_zoom_shortcuts(root);
    return root;
}

/* ================================================================== */
/* TOOL 3 — VIGNETTE                                                  */
/* ================================================================== */

static void vi_refresh(PosterizeState *st) {
    g_clear_object(&st->preview);
    st->preview = apply_vignette(st);
    update_preview(st);
}

static void vi_on_amount(GtkRange *r, gpointer d) {
    PosterizeState *st = get_state(d);
    double v = gtk_range_get_value(r);
    if (fabs(v - st->vig_amount) < 0.5) return;
    push_snapshot(st);
    st->vig_amount = v;
    vi_refresh(st);
}

static void vi_on_radius(GtkRange *r, gpointer d) {
    PosterizeState *st = get_state(d);
    st->vig_radius = gtk_range_get_value(r);
    vi_refresh(st);
}
static void vi_on_feather(GtkRange *r, gpointer d) {
    PosterizeState *st = get_state(d);
    st->vig_feather = gtk_range_get_value(r);
    vi_refresh(st);
}

static void vi_on_color(GObject *dd, GParamSpec *p, gpointer d) {
    (void)p;
    PosterizeState *st = get_state(d);
    st->vig_color = gtk_drop_down_get_selected(GTK_DROP_DOWN(dd));
    vi_refresh(st);
}

static void vi_on_save(GtkButton *b, gpointer d) {
    (void)b; on_save_common(get_state(d), "vignette");
}
static void vi_on_drop(const char *path, gpointer d) {
    PosterizeState *st = get_state(d);
    on_drop_common(st, path);
    if (st->original) vi_refresh(st);
}
static void vi_on_reset(GtkButton *b, gpointer d) {
    (void)b;
    PosterizeState *st = get_state(d);
    push_snapshot(st);
    st->vig_amount = 60;
    st->vig_radius = 60;
    st->vig_feather = 50;
    st->vig_color = 0;
    vi_refresh(st);
}

void image_vignette_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "posterize-state", NULL);
}
static void cmd_vi_reset(GtkWidget *v) { vi_on_reset(NULL, v); }

const HelvetiaToolCommand image_vignette_commands[] = {
    { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic",
      .accel = NULL, .tooltip = "Reset", .activate = cmd_vi_reset },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

GtkWidget *image_vignette_create(void) {
    PosterizeState *st = g_new0(PosterizeState, 1);
    st->undo_stack = g_ptr_array_new();
    st->zoom = 1.0;
    st->vig_amount = 60;
    st->vig_radius = 60;
    st->vig_feather = 50;
    st->vig_color = 0;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;

    GtkWidget *sliders;
    GtkWidget *stack = build_shell(st, &sliders, "Image file",
                                    vi_on_drop, G_CALLBACK(vi_on_save),
                                    G_CALLBACK(vi_on_reset), root);

    /* Color selector */
    GtkWidget *color_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *c_lbl = gtk_label_new("Color:");
    gtk_widget_add_css_class(c_lbl, "dim-label");
    gtk_widget_set_size_request(c_lbl, 130, -1);
    gtk_label_set_xalign(GTK_LABEL(c_lbl), 0.0f);
    const char *colors[] = {"Black", "White", NULL};
    GtkWidget *color_dd = gtk_drop_down_new_from_strings(colors);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(color_dd), 0);
    gtk_box_append(GTK_BOX(color_row), c_lbl);
    gtk_box_append(GTK_BOX(color_row), color_dd);
    gtk_box_append(GTK_BOX(sliders), color_row);

    GtkWidget *s1, *l1, *s2, *l2, *s3, *l3;
    gtk_box_append(GTK_BOX(sliders),
        make_slider("Amount", 0, 100, 1, 60, &s1, &l1,
                     G_CALLBACK(vi_on_amount), root));
    gtk_box_append(GTK_BOX(sliders),
        make_slider("Radius", 0, 100, 1, 60, &s2, &l2,
                     G_CALLBACK(vi_on_radius), root));
    gtk_box_append(GTK_BOX(sliders),
        make_slider("Feather", 1, 100, 1, 50, &s3, &l3,
                     G_CALLBACK(vi_on_feather), root));
    gtk_label_set_text(GTK_LABEL(l1), "60");
    gtk_label_set_text(GTK_LABEL(l2), "60");
    gtk_label_set_text(GTK_LABEL(l3), "50");

    GtkWidget *hint = gtk_label_new(
        "Darkens (or lightens) the corners. "
        "Radius sets where the falloff begins; "
        "Feather controls how soft the transition is.");
    gtk_label_set_wrap(GTK_LABEL(hint), TRUE);
    gtk_widget_add_css_class(hint, "dim-label");
    gtk_widget_add_css_class(hint, "caption");
    gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);
    gtk_box_append(GTK_BOX(sliders), hint);

    g_signal_connect(color_dd, "notify::selected",
                     G_CALLBACK(vi_on_color), root);

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "posterize-state", st,
                           (GDestroyNotify)posterize_state_free);
    image_register_zoom(root, st->picture, &st->zoom);
    image_install_zoom_shortcuts(root);
    return root;
}
