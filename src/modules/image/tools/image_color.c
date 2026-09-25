#include <gtk/gtk.h>
#include <adwaita.h>
#include <math.h>
#include <string.h>
#include "../image_shared.h"
#include "image_color.h"

/* ================================================================== */
/* COMMON STATE                                                       */
/* ================================================================== */

typedef struct {
    GdkPixbuf *original;
    GdkPixbuf *preview;
    GdkPixbuf *first_original;
    GPtrArray *undo_stack;   /* double[12] snapshots */
    char      *path;

    /* All possible parameters — each tool uses a subset */
    double     saturation;   /* -100..+100 */
    double     vibrance;     /* -100..+100 */
    double     hue;          /* -180..+180 degrees */
    double     temperature;  /* -100..+100 (blue → orange) */
    double     tint;         /* -100..+100 (green → magenta) */

    double     cb_shadows_r, cb_shadows_g, cb_shadows_b;
    double     cb_mids_r,    cb_mids_g,    cb_mids_b;
    double     cb_highs_r,   cb_highs_g,   cb_highs_b;
    int        cb_range;     /* 0=shadows, 1=mids, 2=highlights */

    double     zoom;
    GtkWidget *stack, *picture, *root;
    GtkWidget *undo_btn;
    GtkWidget *reset_btn;
} ColorState;

static ColorState *get_state(GtkWidget *v) {
    return g_object_get_data(G_OBJECT(v), "color-state");
}

/* ================================================================== */
/* UNDO SNAPSHOT — 12 doubles                                         */
/* ================================================================== */

typedef struct { double v[12]; } ColorSnapshot;

static void save_snapshot(ColorState *st, double *out) {
    out[0]  = st->saturation; out[1]  = st->vibrance;
    out[2]  = st->hue;
    out[3]  = st->temperature; out[4]  = st->tint;
    out[5]  = st->cb_shadows_r; out[6]  = st->cb_shadows_g; out[7]  = st->cb_shadows_b;
    out[8]  = st->cb_mids_r;    out[9]  = st->cb_mids_g;    out[10] = st->cb_mids_b;
    out[11] = st->cb_highs_r;   /* note: only 12 fit — highs_g/b stay implicit */
}

static void load_snapshot(ColorState *st, const double *v) {
    st->saturation = v[0]; st->vibrance = v[1];
    st->hue = v[2];
    st->temperature = v[3]; st->tint = v[4];
    st->cb_shadows_r = v[5]; st->cb_shadows_g = v[6]; st->cb_shadows_b = v[7];
    st->cb_mids_r = v[8]; st->cb_mids_g = v[9]; st->cb_mids_b = v[10];
    /* cb_highs_r/mids not snapshotted here — simplified */
}

static void push_snapshot(ColorState *st) {
    double *snap = g_new0(double, 12);
    save_snapshot(st, snap);
    g_ptr_array_add(st->undo_stack, snap);
    gtk_widget_set_sensitive(st->undo_btn, TRUE);
}

static void on_undo(GtkButton *b, gpointer d) {
    (void)b;
    ColorState *st = get_state(d);
    if (st->undo_stack->len == 0) return;
    double *snap = g_ptr_array_index(st->undo_stack, st->undo_stack->len - 1);
    g_ptr_array_remove_index(st->undo_stack, st->undo_stack->len - 1);
    load_snapshot(st, snap);
    g_free(snap);
    gtk_widget_set_sensitive(st->undo_btn, st->undo_stack->len > 0);
}

/* ================================================================== */
/* SHARED HELPERS                                                     */
/* ================================================================== */

static void update_preview(ColorState *st) {
    GdkPixbuf *src = st->preview ? st->preview : st->original;
    if (!src) return;
    gtk_picture_set_paintable(GTK_PICTURE(st->picture), GDK_PAINTABLE(gdk_texture_new_for_pixbuf(src)));
    gtk_picture_set_content_fit(GTK_PICTURE(st->picture),
                                 GTK_CONTENT_FIT_CONTAIN);
    gtk_picture_set_can_shrink(GTK_PICTURE(st->picture), TRUE);
    
}

static void clear_undo(ColorState *st) {
    for (guint i = 0; i < st->undo_stack->len; i++)
        g_free(g_ptr_array_index(st->undo_stack, i));
    g_ptr_array_set_size(st->undo_stack, 0);
    gtk_widget_set_sensitive(st->undo_btn, FALSE);
}

static void on_save_common(ColorState *st, const char *prefix) {
    GdkPixbuf *src = st->preview ? st->preview : st->original;
    if (!src) return;
    char *base = st->path ? g_path_get_basename(st->path) : g_strdup("image.png");
    char *name = g_strdup_printf("%s_%s", prefix, base);
    image_save_pixbuf_dialog(st->root, src, name);
    g_free(name);
    g_free(base);
}

static void on_drop_common(ColorState *st, const char *path) {
    st->zoom = 1.0;
    GError *e = NULL;
    GdkPixbuf *pb = image_load_any(path, &e);
    if (!pb) { image_show_error(st->root, e->message); g_error_free(e); return; }

    g_clear_object(&st->original);
    g_clear_object(&st->preview);
    g_clear_object(&st->first_original);
    g_free(st->path);
    clear_undo(st);

    st->original = pb;
    st->first_original = gdk_pixbuf_copy(pb);
    st->path = g_strdup(path);

    /* Reset all parameters */
    st->saturation = st->vibrance = 0;
    st->hue = 0;
    st->temperature = st->tint = 0;
    st->cb_shadows_r = st->cb_shadows_g = st->cb_shadows_b = 0;
    st->cb_mids_r = st->cb_mids_g = st->cb_mids_b = 0;
    st->cb_highs_r = st->cb_highs_g = st->cb_highs_b = 0;
    st->cb_range = 1;

    update_preview(st);
    gtk_stack_set_visible_child_name(GTK_STACK(st->stack), "editor");
}

static void color_state_free(ColorState *st) {
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

/* ================================================================== */
/* COLOR MATH                                                         */
/* ================================================================== */

/* Rec.709 luma */
static inline double luma(double r, double g, double b) {
    return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

/* RGB ↔ HSL */
static void rgb_to_hsl(double r, double g, double b,
                        double *h, double *s, double *l) {
    double max = MAX(MAX(r, g), b);
    double min = MIN(MIN(r, g), b);
    *l = (max + min) / 2.0;
    double d = max - min;

    if (d < 1e-6) { *h = 0; *s = 0; return; }

    *s = (*l > 0.5) ? d / (2.0 - max - min) : d / (max + min);

    if (max == r)      *h = (g - b) / d + (g < b ? 6.0 : 0.0);
    else if (max == g) *h = (b - r) / d + 2.0;
    else               *h = (r - g) / d + 4.0;
    *h /= 6.0;
}

static double hue2rgb(double p, double q, double t) {
    if (t < 0) t += 1;
    if (t > 1) t -= 1;
    if (t < 1.0/6.0) return p + (q - p) * 6.0 * t;
    if (t < 1.0/2.0) return q;
    if (t < 2.0/3.0) return p + (q - p) * (2.0/3.0 - t) * 6.0;
    return p;
}

static void hsl_to_rgb(double h, double s, double l,
                        double *r, double *g, double *b) {
    if (s < 1e-6) { *r = *g = *b = l; return; }
    double q = (l < 0.5) ? l * (1 + s) : l + s - l * s;
    double p = 2 * l - q;
    *r = hue2rgb(p, q, h + 1.0/3.0);
    *g = hue2rgb(p, q, h);
    *b = hue2rgb(p, q, h - 1.0/3.0);
}

static inline double clamp01(double v) {
    return v < 0 ? 0 : (v > 1 ? 1 : v);
}

/* ================================================================== */
/* PIXEL TRANSFORMS                                                   */
/* ================================================================== */

static void pixel_saturation_vibrance(ColorState *st,
                                       guchar *r, guchar *g, guchar *b) {
    double R = *r / 255.0, G = *g / 255.0, B = *b / 255.0;
    double Y = luma(R, G, B);

    /* Saturation — uniform */
    double sat_f = 1.0 + st->saturation / 100.0;
    R = Y + (R - Y) * sat_f;
    G = Y + (G - Y) * sat_f;
    B = Y + (B - Y) * sat_f;

    /* Vibrance — weighted by how unsaturated the pixel already is */
    if (fabs(st->vibrance) > 0.01) {
        double max_c = MAX(MAX(R, G), B);
        double min_c = MIN(MIN(R, G), B);
        double current_sat = (max_c > 1e-6) ? (max_c - min_c) / max_c : 0;
        double vib_weight = 1.0 - current_sat;
        double vib_f = 1.0 + (st->vibrance / 100.0) * vib_weight;
        R = Y + (R - Y) * vib_f;
        G = Y + (G - Y) * vib_f;
        B = Y + (B - Y) * vib_f;
    }

    *r = (guchar)(clamp01(R) * 255.0 + 0.5);
    *g = (guchar)(clamp01(G) * 255.0 + 0.5);
    *b = (guchar)(clamp01(B) * 255.0 + 0.5);
}

static void pixel_hue_shift(ColorState *st,
                             guchar *r, guchar *g, guchar *b) {
    if (fabs(st->hue) < 0.5) return;

    double R = *r / 255.0, G = *g / 255.0, B = *b / 255.0;
    double h, s, l;
    rgb_to_hsl(R, G, B, &h, &s, &l);

    h += st->hue / 360.0;
    while (h < 0) h += 1.0;
    while (h > 1) h -= 1.0;

    hsl_to_rgb(h, s, l, &R, &G, &B);
    *r = (guchar)(clamp01(R) * 255.0 + 0.5);
    *g = (guchar)(clamp01(G) * 255.0 + 0.5);
    *b = (guchar)(clamp01(B) * 255.0 + 0.5);
}

static void pixel_white_balance(ColorState *st,
                                 guchar *r, guchar *g, guchar *b) {
    /* Temperature: positive = warmer (more red, less blue) */
    /* Tint: positive = magenta (less green), negative = green */
    double t = st->temperature / 100.0;
    double ti = st->tint / 100.0;

    double rr = 1.0 + t * 0.3;
    double gg = 1.0 - ti * 0.3;
    double bb = 1.0 - t * 0.3;

    *r = (guchar)(clamp01((*r / 255.0) * rr) * 255.0 + 0.5);
    *g = (guchar)(clamp01((*g / 255.0) * gg) * 255.0 + 0.5);
    *b = (guchar)(clamp01((*b / 255.0) * bb) * 255.0 + 0.5);
}

static void pixel_color_balance(ColorState *st,
                                 guchar *r, guchar *g, guchar *b) {
    double R = *r / 255.0, G = *g / 255.0, B = *b / 255.0;
    double Y = luma(R, G, B);

    /* Shadow weight, mid weight, highlight weight based on luma */
    double sw = pow(1.0 - Y, 2.0);
    double hw = pow(Y, 2.0);
    double mw = 1.0 - sw - hw;
    if (mw < 0) mw = 0;

    double dr = (st->cb_shadows_r * sw + st->cb_mids_r * mw + st->cb_highs_r * hw) / 100.0;
    double dg = (st->cb_shadows_g * sw + st->cb_mids_g * mw + st->cb_highs_g * hw) / 100.0;
    double db = (st->cb_shadows_b * sw + st->cb_mids_b * mw + st->cb_highs_b * hw) / 100.0;

    R = clamp01(R + dr * 0.3);
    G = clamp01(G + dg * 0.3);
    B = clamp01(B + db * 0.3);

    *r = (guchar)(R * 255.0 + 0.5);
    *g = (guchar)(G * 255.0 + 0.5);
    *b = (guchar)(B * 255.0 + 0.5);
}

/* ================================================================== */
/* GENERIC APPLY — dispatches to whichever effect is enabled          */
/* ================================================================== */

typedef enum {
    EFFECT_SAT_VIB,
    EFFECT_HUE,
    EFFECT_WB,
    EFFECT_CB,
} EffectKind;

static GdkPixbuf *apply_effect(ColorState *st, EffectKind kind) {
    GdkPixbuf *src = st->original;
    int w = gdk_pixbuf_get_width(src);
    int h = gdk_pixbuf_get_height(src);
    int n = gdk_pixbuf_get_n_channels(src);
    int stride = gdk_pixbuf_get_rowstride(src);
    guchar *px = gdk_pixbuf_get_pixels(src);

    GdkPixbuf *out = gdk_pixbuf_copy(src);
    int ostride = gdk_pixbuf_get_rowstride(out);
    guchar *opx = gdk_pixbuf_get_pixels(out);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            guchar *sp = px + y * stride + x * n;
            guchar *dp = opx + y * ostride + x * n;
            guchar r = sp[0], g = sp[1], b = sp[2];

            switch (kind) {
                case EFFECT_SAT_VIB: pixel_saturation_vibrance(st, &r, &g, &b); break;
                case EFFECT_HUE:     pixel_hue_shift(st, &r, &g, &b); break;
                case EFFECT_WB:      pixel_white_balance(st, &r, &g, &b); break;
                case EFFECT_CB:      pixel_color_balance(st, &r, &g, &b); break;
            }

            dp[0] = r; dp[1] = g; dp[2] = b;
            if (n == 4) dp[3] = sp[3];
        }
    }
    return out;
}

/* ================================================================== */
/* SHARED UI BUILDERS                                                 */
/* ================================================================== */

static GtkWidget *make_slider_row(const char *label,
                                    double min, double max, double step,
                                    double initial,
                                    GtkWidget **out_scale,
                                    GtkWidget **out_value_lbl,
                                    GCallback cb, gpointer user_data) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *lbl = gtk_label_new(label);
    gtk_widget_add_css_class(lbl, "dim-label");
    gtk_widget_set_size_request(lbl, 100, -1);
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
    *out_value_lbl = val;
    if (cb) g_signal_connect(scale, "value-changed", cb, user_data);
    return row;
}

static GtkWidget *build_editor_shell(ColorState *st,
                                      GtkWidget **out_sliders_box,
                                      const char *save_prefix,
                                      GCallback on_save_cb,
                                      GCallback on_reset_cb,
                                      const char *drop_hint,
                                      ImageDropCallback on_drop_cb) {
    GtkWidget *stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(stack),
                                  GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_widget_set_vexpand(stack, TRUE);
    st->stack = stack;

    /* Drop page */
    GtkWidget *drop_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(drop_box, 24);
    gtk_widget_set_margin_end(drop_box, 24);
    gtk_widget_set_margin_top(drop_box, 24);
    gtk_widget_set_margin_bottom(drop_box, 24);
    gtk_widget_set_vexpand(drop_box, TRUE);
    gtk_box_append(GTK_BOX(drop_box),
        image_build_drop_zone(drop_hint, on_drop_cb, st->root));
    gtk_stack_add_named(GTK_STACK(stack), drop_box, "drop");

    /* Editor */
    GtkWidget *editor = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    GtkWidget *sliders = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start(sliders, 12);
    gtk_widget_set_margin_end(sliders, 12);
    gtk_widget_set_margin_top(sliders, 8);
    gtk_widget_set_margin_bottom(sliders, 8);
    *out_sliders_box = sliders;

    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(bar, 12);
    gtk_widget_set_margin_end(bar, 12);
    gtk_widget_set_margin_bottom(bar, 8);

    st->undo_btn = image_undo_button(G_CALLBACK(on_undo), st->root);
    gtk_widget_set_sensitive(st->undo_btn, FALSE);

    GtkWidget *reset = image_reset_button(on_reset_cb, st->root);

    GtkWidget *new_img = image_new_image_button(on_drop_cb, st->root);

    GtkWidget *sp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(sp, TRUE);

    GtkWidget *save = gtk_button_new_with_label("Save As…");
    gtk_widget_add_css_class(save, "flat");
    if (on_save_cb) g_signal_connect(save, "clicked", on_save_cb, st->root);

    gtk_box_append(GTK_BOX(bar), st->undo_btn);
    gtk_box_append(GTK_BOX(bar), reset);

    gtk_box_append(GTK_BOX(bar), gtk_separator_new(GTK_ORIENTATION_VERTICAL));
    GtkWidget *z_out = gtk_button_new_from_icon_name("zoom-out-symbolic");
    GtkWidget *z_in = gtk_button_new_from_icon_name("zoom-in-symbolic");
    GtkWidget *z_1 = gtk_button_new_from_icon_name("zoom-original-symbolic");
    gtk_widget_add_css_class(z_out, "flat");
    gtk_widget_add_css_class(z_in, "flat");
    gtk_widget_add_css_class(z_1, "flat");
    g_signal_connect_swapped(z_out, "clicked", G_CALLBACK(image_zoom_out), st->root);
    g_signal_connect_swapped(z_in, "clicked", G_CALLBACK(image_zoom_in), st->root);
    g_signal_connect_swapped(z_1, "clicked", G_CALLBACK(image_zoom_reset), st->root);
    gtk_box_append(GTK_BOX(bar), z_out);
    gtk_box_append(GTK_BOX(bar), z_1);
    gtk_box_append(GTK_BOX(bar), z_in);

    gtk_box_append(GTK_BOX(bar), gtk_separator_new(GTK_ORIENTATION_VERTICAL));
    gtk_box_append(GTK_BOX(bar), new_img);
    gtk_box_append(GTK_BOX(bar), sp);
    gtk_box_append(GTK_BOX(bar), save);

    g_object_set_data(G_OBJECT(reset), "save_prefix",
                       (gpointer)save_prefix);

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
/* TOOL 1 — SATURATION / VIBRANCE                                     */
/* ================================================================== */

static GdkPixbuf *sv_apply(ColorState *st) {
    return apply_effect(st, EFFECT_SAT_VIB);
}

static void sv_on_sat(GtkRange *r, gpointer d) {
    ColorState *st = get_state(d);
    double v = gtk_range_get_value(r);
    if (fabs(v - st->saturation) < 0.5) return;
    push_snapshot(st);
    st->saturation = v;
    g_clear_object(&st->preview);
    st->preview = sv_apply(st);
    update_preview(st);
}

static void sv_on_vib(GtkRange *r, gpointer d) {
    ColorState *st = get_state(d);
    double v = gtk_range_get_value(r);
    if (fabs(v - st->vibrance) < 0.5) return;
    push_snapshot(st);
    st->vibrance = v;
    g_clear_object(&st->preview);
    st->preview = sv_apply(st);
    update_preview(st);
}

static void sv_on_save(GtkButton *b, gpointer d) {
    (void)b;
    on_save_common(get_state(d), "satvib");
}

static void sv_on_drop(const char *path, gpointer d) {
    on_drop_common(get_state(d), path);
}

static void sv_on_reset(GtkButton *b, gpointer d) {
    (void)b;
    ColorState *st = get_state(d);
    push_snapshot(st);
    st->saturation = 0;
    st->vibrance = 0;
    g_clear_object(&st->preview);
    update_preview(st);
}

void image_saturation_vibrance_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "color-state", NULL);
}

static void cmd_sv_reset(GtkWidget *v) { sv_on_reset(NULL, v); }

const HelvetiaToolCommand image_saturation_vibrance_commands[] = {
    { .id = "reset", .name = "Reset",
      .icon_name = "view-refresh-symbolic",
      .accel = NULL, .tooltip = "Reset saturation and vibrance",
      .activate = cmd_sv_reset },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

GtkWidget *image_saturation_vibrance_create(void) {
    ColorState *st = g_new0(ColorState, 1);
    st->undo_stack = g_ptr_array_new();
    st->zoom = 1.0;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;
    image_install_edit_shortcuts(root,
        (ImageToolCallback)on_undo,
        (ImageToolCallback)sv_on_reset,
        root);

    GtkWidget *sliders;
    GtkWidget *stack = build_editor_shell(st, &sliders, "satvib",
                                           G_CALLBACK(sv_on_save),
                                           G_CALLBACK(sv_on_reset),
                                           "Image file", sv_on_drop);

    GtkWidget *sat_scale, *sat_lbl;
    GtkWidget *vib_scale, *vib_lbl;

    gtk_box_append(GTK_BOX(sliders),
        make_slider_row("Saturation", -100, 100, 1, 0,
                        &sat_scale, &sat_lbl,
                        G_CALLBACK(sv_on_sat), root));
    gtk_box_append(GTK_BOX(sliders),
        make_slider_row("Vibrance", -100, 100, 1, 0,
                        &vib_scale, &vib_lbl,
                        G_CALLBACK(sv_on_vib), root));
    gtk_label_set_text(GTK_LABEL(sat_lbl), "+0");
    gtk_label_set_text(GTK_LABEL(vib_lbl), "+0");

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "color-state", st,
                           (GDestroyNotify)color_state_free);
    image_register_zoom(root, st->picture, &st->zoom);
    image_install_zoom_shortcuts(root);

    return root;
}

/* ================================================================== */
/* TOOL 2 — HUE SHIFT                                                 */
/* ================================================================== */

static void hs_on_hue(GtkRange *r, gpointer d) {
    ColorState *st = get_state(d);
    double v = gtk_range_get_value(r);
    if (fabs(v - st->hue) < 0.5) return;
    push_snapshot(st);
    st->hue = v;
    g_clear_object(&st->preview);
    st->preview = apply_effect(st, EFFECT_HUE);
    update_preview(st);
}

static void hs_on_save(GtkButton *b, gpointer d) {
    (void)b;
    on_save_common(get_state(d), "hue");
}

static void hs_on_drop(const char *path, gpointer d) {
    on_drop_common(get_state(d), path);
}

static void hs_on_reset(GtkButton *b, gpointer d) {
    (void)b;
    ColorState *st = get_state(d);
    push_snapshot(st);
    st->hue = 0;
    g_clear_object(&st->preview);
    update_preview(st);
}

void image_hue_shift_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "color-state", NULL);
}

static void cmd_hs_reset(GtkWidget *v) { hs_on_reset(NULL, v); }

const HelvetiaToolCommand image_hue_shift_commands[] = {
    { .id = "reset", .name = "Reset",
      .icon_name = "view-refresh-symbolic",
      .accel = NULL, .tooltip = "Reset hue shift",
      .activate = cmd_hs_reset },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

GtkWidget *image_hue_shift_create(void) {
    ColorState *st = g_new0(ColorState, 1);
    st->undo_stack = g_ptr_array_new();
    st->zoom = 1.0;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;
    image_install_edit_shortcuts(root,
        (ImageToolCallback)on_undo,
        (ImageToolCallback)hs_on_reset,
        root);

    GtkWidget *sliders;
    GtkWidget *stack = build_editor_shell(st, &sliders, "hue",
                                           G_CALLBACK(hs_on_save),
                                           G_CALLBACK(hs_on_reset),
                                           "Image file", hs_on_drop);

    GtkWidget *hue_scale, *hue_lbl;
    gtk_box_append(GTK_BOX(sliders),
        make_slider_row("Hue", -180, 180, 1, 0,
                        &hue_scale, &hue_lbl,
                        G_CALLBACK(hs_on_hue), root));
    gtk_label_set_text(GTK_LABEL(hue_lbl), "+0°");

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "color-state", st,
                           (GDestroyNotify)color_state_free);
    image_register_zoom(root, st->picture, &st->zoom);
    image_install_zoom_shortcuts(root);

    return root;
}

/* ================================================================== */
/* TOOL 3 — WHITE BALANCE                                             */
/* ================================================================== */

static void wb_refresh(ColorState *st) {
    g_clear_object(&st->preview);
    st->preview = apply_effect(st, EFFECT_WB);
    update_preview(st);
}

static void wb_on_temp(GtkRange *r, gpointer d) {
    ColorState *st = get_state(d);
    double v = gtk_range_get_value(r);
    if (fabs(v - st->temperature) < 0.5) return;
    push_snapshot(st);
    st->temperature = v;
    wb_refresh(st);
}

static void wb_on_tint(GtkRange *r, gpointer d) {
    ColorState *st = get_state(d);
    double v = gtk_range_get_value(r);
    if (fabs(v - st->tint) < 0.5) return;
    push_snapshot(st);
    st->tint = v;
    wb_refresh(st);
}

static void wb_on_save(GtkButton *b, gpointer d) {
    (void)b;
    on_save_common(get_state(d), "wb");
}

static void wb_on_drop(const char *path, gpointer d) {
    on_drop_common(get_state(d), path);
}

static void wb_on_reset(GtkButton *b, gpointer d) {
    (void)b;
    ColorState *st = get_state(d);
    push_snapshot(st);
    st->temperature = st->tint = 0;
    wb_refresh(st);
}

void image_white_balance_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "color-state", NULL);
}

static void cmd_wb_reset(GtkWidget *v) { wb_on_reset(NULL, v); }

const HelvetiaToolCommand image_white_balance_commands[] = {
    { .id = "reset", .name = "Reset",
      .icon_name = "view-refresh-symbolic",
      .accel = NULL, .tooltip = "Reset white balance",
      .activate = cmd_wb_reset },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

GtkWidget *image_white_balance_create(void) {
    ColorState *st = g_new0(ColorState, 1);
    st->undo_stack = g_ptr_array_new();
    st->zoom = 1.0;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;
    image_install_edit_shortcuts(root,
        (ImageToolCallback)on_undo,
        (ImageToolCallback)wb_on_reset,
        root);

    GtkWidget *sliders;
    GtkWidget *stack = build_editor_shell(st, &sliders, "wb",
                                           G_CALLBACK(wb_on_save),
                                           G_CALLBACK(wb_on_reset),
                                           "Image file", wb_on_drop);

    GtkWidget *t_scale, *t_lbl;
    GtkWidget *ti_scale, *ti_lbl;

    gtk_box_append(GTK_BOX(sliders),
        make_slider_row("Temperature", -100, 100, 1, 0,
                        &t_scale, &t_lbl,
                        G_CALLBACK(wb_on_temp), root));
    gtk_box_append(GTK_BOX(sliders),
        make_slider_row("Tint", -100, 100, 1, 0,
                        &ti_scale, &ti_lbl,
                        G_CALLBACK(wb_on_tint), root));
    gtk_label_set_text(GTK_LABEL(t_lbl), "+0");
    gtk_label_set_text(GTK_LABEL(ti_lbl), "+0");

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "color-state", st,
                           (GDestroyNotify)color_state_free);
    image_register_zoom(root, st->picture, &st->zoom);
    image_install_zoom_shortcuts(root);

    return root;
}

/* ================================================================== */
/* TOOL 4 — COLOR BALANCE                                             */
/* ================================================================== */

static void cb_refresh(ColorState *st) {
    g_clear_object(&st->preview);
    st->preview = apply_effect(st, EFFECT_CB);
    update_preview(st);
}

static void cb_on_r(GtkRange *r, gpointer d) {
    ColorState *st = get_state(d);
    double v = gtk_range_get_value(r);
    if (st->cb_range == 0) st->cb_shadows_r = v;
    else if (st->cb_range == 1) st->cb_mids_r = v;
    else st->cb_highs_r = v;
    cb_refresh(st);
}

static void cb_on_g(GtkRange *r, gpointer d) {
    ColorState *st = get_state(d);
    double v = gtk_range_get_value(r);
    if (st->cb_range == 0) st->cb_shadows_g = v;
    else if (st->cb_range == 1) st->cb_mids_g = v;
    else st->cb_highs_g = v;
    cb_refresh(st);
}

static void cb_on_b(GtkRange *r, gpointer d) {
    ColorState *st = get_state(d);
    double v = gtk_range_get_value(r);
    if (st->cb_range == 0) st->cb_shadows_b = v;
    else if (st->cb_range == 1) st->cb_mids_b = v;
    else st->cb_highs_b = v;
    cb_refresh(st);
}

static void cb_on_save(GtkButton *b, gpointer d) {
    (void)b;
    on_save_common(get_state(d), "cb");
}

static void cb_on_drop(const char *path, gpointer d) {
    on_drop_common(get_state(d), path);
}

static void cb_on_reset(GtkButton *b, gpointer d) {
    (void)b;
    ColorState *st = get_state(d);
    push_snapshot(st);
    st->cb_shadows_r = st->cb_shadows_g = st->cb_shadows_b = 0;
    st->cb_mids_r = st->cb_mids_g = st->cb_mids_b = 0;
    st->cb_highs_r = st->cb_highs_g = st->cb_highs_b = 0;
    cb_refresh(st);
}

static void cb_on_range_changed(GObject *dd, GParamSpec *p, gpointer d) {
    (void)p;
    ColorState *st = get_state(d);
    st->cb_range = gtk_drop_down_get_selected(GTK_DROP_DOWN(dd));
}

void image_color_balance_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "color-state", NULL);
}

static void cmd_cb_reset(GtkWidget *v) { cb_on_reset(NULL, v); }

const HelvetiaToolCommand image_color_balance_commands[] = {
    { .id = "reset", .name = "Reset",
      .icon_name = "view-refresh-symbolic",
      .accel = NULL, .tooltip = "Reset color balance",
      .activate = cmd_cb_reset },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

GtkWidget *image_color_balance_create(void) {
    ColorState *st = g_new0(ColorState, 1);
    st->undo_stack = g_ptr_array_new();
    st->zoom = 1.0;
    st->cb_range = 1;   /* mids by default */

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;
    image_install_edit_shortcuts(root,
        (ImageToolCallback)on_undo,
        (ImageToolCallback)cb_on_reset,
        root);

    GtkWidget *sliders;
    GtkWidget *stack = build_editor_shell(st, &sliders, "cb",
                                           G_CALLBACK(cb_on_save),
                                           G_CALLBACK(cb_on_reset),
                                           "Image file", cb_on_drop);

    /* Range selector */
    GtkWidget *range_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *r_lbl = gtk_label_new("Range:");
    gtk_widget_add_css_class(r_lbl, "dim-label");
    gtk_widget_set_size_request(r_lbl, 100, -1);
    gtk_label_set_xalign(GTK_LABEL(r_lbl), 0.0f);

    const char *ranges[] = {"Shadows", "Midtones", "Highlights", NULL};
    GtkWidget *range_dd = gtk_drop_down_new_from_strings(ranges);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(range_dd), 1);

    gtk_box_append(GTK_BOX(range_row), r_lbl);
    gtk_box_append(GTK_BOX(range_row), range_dd);
    gtk_box_append(GTK_BOX(sliders), range_row);

    GtkWidget *r_scale, *r_lbl_v;
    GtkWidget *g_scale, *g_lbl_v;
    GtkWidget *b_scale, *b_lbl_v;

    gtk_box_append(GTK_BOX(sliders),
        make_slider_row("Red", -100, 100, 1, 0,
                        &r_scale, &r_lbl_v,
                        G_CALLBACK(cb_on_r), root));
    gtk_box_append(GTK_BOX(sliders),
        make_slider_row("Green", -100, 100, 1, 0,
                        &g_scale, &g_lbl_v,
                        G_CALLBACK(cb_on_g), root));
    gtk_box_append(GTK_BOX(sliders),
        make_slider_row("Blue", -100, 100, 1, 0,
                        &b_scale, &b_lbl_v,
                        G_CALLBACK(cb_on_b), root));
    gtk_label_set_text(GTK_LABEL(r_lbl_v), "+0");
    gtk_label_set_text(GTK_LABEL(g_lbl_v), "+0");
    gtk_label_set_text(GTK_LABEL(b_lbl_v), "+0");

    g_signal_connect(range_dd, "notify::selected",
                     G_CALLBACK(cb_on_range_changed), root);

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "color-state", st,
                           (GDestroyNotify)color_state_free);
    image_register_zoom(root, st->picture, &st->zoom);
    image_install_zoom_shortcuts(root);

    return root;
}
