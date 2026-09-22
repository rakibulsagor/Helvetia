#include <gtk/gtk.h>
#include <adwaita.h>
#include <math.h>
#include <string.h>
#include "../image_shared.h"
#include "image_tone.h"

/* ================================================================== */
/* SHARED STATE                                                       */
/* ================================================================== */

typedef struct {
    GdkPixbuf *original;
    GdkPixbuf *preview;
    GdkPixbuf *first_original;
    GPtrArray *undo_stack;   /* double[4] */
    char      *path;

    /* Shadows/Highlights */
    double     shadows;      /* -100..+100 */
    double     highlights;   /* -100..+100 */
    double     sh_width;     /* 0..100 tonal width for shadows */
    double     hi_width;     /* 0..100 tonal width for highlights */

    /* Gamma */
    double     gamma;        /* 0.1..5.0 */

    /* Auto Enhance */
    double     ae_strength;  /* 0..100 */
    gboolean   ae_auto_level;
    gboolean   ae_auto_wb;
    gboolean   ae_auto_sat;

    double     zoom;
    GtkWidget *stack, *picture, *overlay, *root;
    GtkWidget *undo_btn;
} ToneState;

static ToneState *get_state(GtkWidget *v) {
    return g_object_get_data(G_OBJECT(v), "tone-state");
}

/* ================================================================== */
/* UNDO                                                               */
/* ================================================================== */

static void snapshot_save(ToneState *st, double v[4]) {
    v[0] = st->shadows;
    v[1] = st->highlights;
    v[2] = st->sh_width;
    v[3] = st->hi_width;
}

static void snapshot_load(ToneState *st, const double v[4]) {
    st->shadows = v[0];
    st->highlights = v[1];
    st->sh_width = v[2];
    st->hi_width = v[3];
}

static void push_snapshot(ToneState *st) {
    double *s = g_new0(double, 4);
    snapshot_save(st, s);
    g_ptr_array_add(st->undo_stack, s);
    gtk_widget_set_sensitive(st->undo_btn, TRUE);
}

static void on_undo(GtkButton *b, gpointer d) {
    (void)b;
    ToneState *st = get_state(d);
    if (st->undo_stack->len == 0) return;
    double *s = g_ptr_array_index(st->undo_stack, st->undo_stack->len - 1);
    g_ptr_array_remove_index(st->undo_stack, st->undo_stack->len - 1);
    snapshot_load(st, s);
    g_free(s);
    gtk_widget_set_sensitive(st->undo_btn, st->undo_stack->len > 0);
}

static void clear_undo(ToneState *st) {
    for (guint i = 0; i < st->undo_stack->len; i++)
        g_free(g_ptr_array_index(st->undo_stack, i));
    g_ptr_array_set_size(st->undo_stack, 0);
    gtk_widget_set_sensitive(st->undo_btn, FALSE);
}

/* ================================================================== */
/* SHARED HELPERS                                                     */
/* ================================================================== */

static void update_preview(ToneState *st) {
    GdkPixbuf *src = st->preview ? st->preview : st->original;
    if (!src) return;
    GdkTexture *t = gdk_texture_new_for_pixbuf(src);
    gtk_picture_set_paintable(GTK_PICTURE(st->picture), GDK_PAINTABLE(t));
    g_object_unref(t);
}

static void on_save_common(ToneState *st, const char *prefix) {
    GdkPixbuf *src = st->preview ? st->preview : st->original;
    if (!src) return;
    char *base = st->path ? g_path_get_basename(st->path) : g_strdup("image.png");
    char *name = g_strdup_printf("%s_%s", prefix, base);
    image_save_pixbuf_dialog(st->root, src, name);
    g_free(name);
    g_free(base);
}

static void on_drop_common(ToneState *st, const char *path) {
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

    /* Defaults */
    st->shadows = st->highlights = 0;
    st->sh_width = 50; st->hi_width = 50;
    st->gamma = 1.0;
    st->ae_strength = 50;
    st->ae_auto_level = TRUE;
    st->ae_auto_wb = TRUE;
    st->ae_auto_sat = TRUE;

    update_preview(st);
    gtk_stack_set_visible_child_name(GTK_STACK(st->stack), "editor");
}

static void tone_state_free(ToneState *st) {
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

static inline double clamp01d(double v) {
    return v < 0 ? 0 : (v > 1 ? 1 : v);
}
static inline double luma709(double r, double g, double b) {
    return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

/* ================================================================== */
/* EFFECTS                                                            */
/* ================================================================== */

static GdkPixbuf *apply_shadows_highlights(ToneState *st) {
    GdkPixbuf *src = st->original;
    int w = gdk_pixbuf_get_width(src);
    int h = gdk_pixbuf_get_height(src);
    int n = gdk_pixbuf_get_n_channels(src);
    int stride = gdk_pixbuf_get_rowstride(src);
    guchar *px = gdk_pixbuf_get_pixels(src);

    GdkPixbuf *out = gdk_pixbuf_copy(src);
    int ostride = gdk_pixbuf_get_rowstride(out);
    guchar *opx = gdk_pixbuf_get_pixels(out);

    /* Shadow weight curve: lower luma → stronger shadow mask */
    double sh_f = st->shadows / 100.0;
    double hi_f = st->highlights / 100.0;
    double sh_pow = 1.0 + (100.0 - st->sh_width) / 100.0;    /* 1..2 */
    double hi_pow = 1.0 + (100.0 - st->hi_width) / 100.0;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            guchar *sp = px + y * stride + x * n;
            guchar *dp = opx + y * ostride + x * n;

            double R = sp[0] / 255.0, G = sp[1] / 255.0, B = sp[2] / 255.0;
            double Y = luma709(R, G, B);

            /* Shadow mask: peaks at Y=0, falls off toward Y=1 */
            double shadow_mask = pow(1.0 - Y, sh_pow);
            /* Highlight mask: peaks at Y=1, falls off toward Y=0 */
            double highlight_mask = pow(Y, hi_pow);

            /* Positive shadows = lift; negative = crush */
            double shadow_delta = sh_f * shadow_mask * 0.5;
            double highlight_delta = hi_f * highlight_mask * 0.5;

            /* Apply as additive to each channel with luma-aware weighting */
            double newR = R + shadow_delta * (1.0 - R) + highlight_delta * (1.0 - R);
            double newG = G + shadow_delta * (1.0 - G) + highlight_delta * (1.0 - G);
            double newB = B + shadow_delta * (1.0 - B) + highlight_delta * (1.0 - B);

            dp[0] = (guchar)(clamp01d(newR) * 255.0 + 0.5);
            dp[1] = (guchar)(clamp01d(newG) * 255.0 + 0.5);
            dp[2] = (guchar)(clamp01d(newB) * 255.0 + 0.5);
            if (n == 4) dp[3] = sp[3];
        }
    }
    return out;
}

static GdkPixbuf *apply_gamma(ToneState *st) {
    GdkPixbuf *src = st->original;
    int w = gdk_pixbuf_get_width(src);
    int h = gdk_pixbuf_get_height(src);
    int n = gdk_pixbuf_get_n_channels(src);
    int stride = gdk_pixbuf_get_rowstride(src);
    guchar *px = gdk_pixbuf_get_pixels(src);

    GdkPixbuf *out = gdk_pixbuf_copy(src);
    int ostride = gdk_pixbuf_get_rowstride(out);
    guchar *opx = gdk_pixbuf_get_pixels(out);

    double inv = 1.0 / st->gamma;
    guchar lut[256];
    for (int i = 0; i < 256; i++) {
        double v = i / 255.0;
        v = pow(v, inv);
        lut[i] = (guchar)(clamp01d(v) * 255.0 + 0.5);
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

/* Compute per-channel histogram + percentile clip for auto level */
static GdkPixbuf *apply_auto_enhance(ToneState *st) {
    GdkPixbuf *src = st->original;
    int w = gdk_pixbuf_get_width(src);
    int h = gdk_pixbuf_get_height(src);
    int n = gdk_pixbuf_get_n_channels(src);
    int stride = gdk_pixbuf_get_rowstride(src);
    guchar *px = gdk_pixbuf_get_pixels(src);

    double strength = st->ae_strength / 100.0;

    /* --- 1. Compute per-channel histogram for auto level --- */
    guint hist_r[256] = {0}, hist_g[256] = {0}, hist_b[256] = {0};
    int total = w * h;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            guchar *p = px + y * stride + x * n;
            hist_r[p[0]]++;
            hist_g[p[1]]++;
            hist_b[p[2]]++;
        }
    }

    /* Find 0.5% and 99.5% percentile as black/white points */
    int lo_r = 0, hi_r = 255, lo_g = 0, hi_g = 255, lo_b = 0, hi_b = 255;
    int clip = total / 200;   /* 0.5% */

    if (st->ae_auto_level) {
        guint cum = 0;
        for (int i = 0; i < 256; i++) { cum += hist_r[i]; if ((int)cum > clip) { lo_r = i; break; } }
        cum = 0;
        for (int i = 255; i >= 0; i--) { cum += hist_r[i]; if ((int)cum > clip) { hi_r = i; break; } }
        cum = 0;
        for (int i = 0; i < 256; i++) { cum += hist_g[i]; if ((int)cum > clip) { lo_g = i; break; } }
        cum = 0;
        for (int i = 255; i >= 0; i--) { cum += hist_g[i]; if ((int)cum > clip) { hi_g = i; break; } }
        cum = 0;
        for (int i = 0; i < 256; i++) { cum += hist_b[i]; if ((int)cum > clip) { lo_b = i; break; } }
        cum = 0;
        for (int i = 255; i >= 0; i--) { cum += hist_b[i]; if ((int)cum > clip) { hi_b = i; break; } }

        /* Blend with neutral (0,255) by strength */
        lo_r = (int)(lo_r * strength + 0 * (1.0 - strength));
        hi_r = (int)(hi_r * strength + 255 * (1.0 - strength));
        lo_g = (int)(lo_g * strength + 0 * (1.0 - strength));
        hi_g = (int)(hi_g * strength + 255 * (1.0 - strength));
        lo_b = (int)(lo_b * strength + 0 * (1.0 - strength));
        hi_b = (int)(hi_b * strength + 255 * (1.0 - strength));
    }

    /* --- 2. Auto WB (gray-world) --- */
    double wb_r = 1.0, wb_g = 1.0, wb_b = 1.0;
    if (st->ae_auto_wb) {
        double sum_r = 0, sum_g = 0, sum_b = 0;
        for (int i = 0; i < 256; i++) {
            sum_r += i * hist_r[i];
            sum_g += i * hist_g[i];
            sum_b += i * hist_b[i];
        }
        double avg_r = sum_r / total;
        double avg_g = sum_g / total;
        double avg_b = sum_b / total;
        double gray = (avg_r + avg_g + avg_b) / 3.0;
        if (avg_r > 1) wb_r = 1.0 + (gray / avg_r - 1.0) * strength;
        if (avg_g > 1) wb_g = 1.0 + (gray / avg_g - 1.0) * strength;
        if (avg_b > 1) wb_b = 1.0 + (gray / avg_b - 1.0) * strength;
    }

    /* --- 3. Build LUTs --- */
    guchar lut_r[256], lut_g[256], lut_b[256];
    double range_r = hi_r - lo_r; if (range_r < 1) range_r = 1;
    double range_g = hi_g - lo_g; if (range_g < 1) range_g = 1;
    double range_b = hi_b - lo_b; if (range_b < 1) range_b = 1;

    for (int i = 0; i < 256; i++) {
        double r = ((double)i - lo_r) / range_r;
        double g = ((double)i - lo_g) / range_g;
        double b = ((double)i - lo_b) / range_b;
        r *= wb_r; g *= wb_g; b *= wb_b;
        lut_r[i] = (guchar)(clamp01d(r) * 255.0 + 0.5);
        lut_g[i] = (guchar)(clamp01d(g) * 255.0 + 0.5);
        lut_b[i] = (guchar)(clamp01d(b) * 255.0 + 0.5);
    }

    /* --- 4. Apply --- */
    GdkPixbuf *out = gdk_pixbuf_copy(src);
    int ostride = gdk_pixbuf_get_rowstride(out);
    guchar *opx = gdk_pixbuf_get_pixels(out);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            guchar *sp = px + y * stride + x * n;
            guchar *dp = opx + y * ostride + x * n;

            double R = lut_r[sp[0]] / 255.0;
            double G = lut_g[sp[1]] / 255.0;
            double B = lut_b[sp[2]] / 255.0;

            /* Auto saturation — subtle boost */
            if (st->ae_auto_sat && strength > 0.01) {
                double Y = luma709(R, G, B);
                double sat_boost = 1.0 + 0.15 * strength;
                R = Y + (R - Y) * sat_boost;
                G = Y + (G - Y) * sat_boost;
                B = Y + (B - Y) * sat_boost;
            }

            dp[0] = (guchar)(clamp01d(R) * 255.0 + 0.5);
            dp[1] = (guchar)(clamp01d(G) * 255.0 + 0.5);
            dp[2] = (guchar)(clamp01d(B) * 255.0 + 0.5);
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
    gtk_widget_set_size_request(lbl, 110, -1);
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

static GtkWidget *build_shell(ToneState *st, GtkWidget **out_sliders,
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

    /* Drop page */
    GtkWidget *drop_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(drop_box, 24);
    gtk_widget_set_margin_end(drop_box, 24);
    gtk_widget_set_margin_top(drop_box, 24);
    gtk_widget_set_margin_bottom(drop_box, 24);
    gtk_widget_set_vexpand(drop_box, TRUE);
    gtk_box_append(GTK_BOX(drop_box),
        image_build_drop_zone(drop_hint, on_drop, root));
    gtk_stack_add_named(GTK_STACK(stack), drop_box, "drop");

    /* Editor */
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

    GtkWidget *pic = gtk_picture_new();
    gtk_picture_set_can_shrink(GTK_PICTURE(pic), FALSE);
    gtk_picture_set_content_fit(GTK_PICTURE(pic), GTK_CONTENT_FIT_CONTAIN);
    gtk_widget_add_css_class(pic, "image-viewer-canvas");
    st->picture = pic;

    GtkWidget *scroller = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), pic);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    GtkWidget *overlay = gtk_overlay_new();
    gtk_overlay_set_child(GTK_OVERLAY(overlay), scroller);
    gtk_widget_set_vexpand(overlay, TRUE);
    st->overlay = overlay;

    gtk_box_append(GTK_BOX(editor), sliders);
    gtk_box_append(GTK_BOX(editor), bar);
    gtk_box_append(GTK_BOX(editor),
                   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(editor), overlay);
    gtk_stack_add_named(GTK_STACK(stack), editor, "editor");
    gtk_stack_set_visible_child_name(GTK_STACK(stack), "drop");

    return stack;
}

/* ================================================================== */
/* TOOL 1 — SHADOWS / HIGHLIGHTS                                      */
/* ================================================================== */

static void sh_refresh(ToneState *st) {
    g_clear_object(&st->preview);
    st->preview = apply_shadows_highlights(st);
    update_preview(st);
}

static void sh_on_shadows(GtkRange *r, gpointer d) {
    ToneState *st = get_state(d);
    double v = gtk_range_get_value(r);
    if (fabs(v - st->shadows) < 0.5) return;
    push_snapshot(st);
    st->shadows = v;
    sh_refresh(st);
}
static void sh_on_highlights(GtkRange *r, gpointer d) {
    ToneState *st = get_state(d);
    double v = gtk_range_get_value(r);
    if (fabs(v - st->highlights) < 0.5) return;
    push_snapshot(st);
    st->highlights = v;
    sh_refresh(st);
}
static void sh_on_shw(GtkRange *r, gpointer d) {
    ToneState *st = get_state(d);
    st->sh_width = gtk_range_get_value(r);
    sh_refresh(st);
}
static void sh_on_hiw(GtkRange *r, gpointer d) {
    ToneState *st = get_state(d);
    st->hi_width = gtk_range_get_value(r);
    sh_refresh(st);
}

static void sh_on_save(GtkButton *b, gpointer d) {
    (void)b; on_save_common(get_state(d), "sh");
}
static void sh_on_drop(const char *path, gpointer d) {
    on_drop_common(get_state(d), path);
}
static void sh_on_reset(GtkButton *b, gpointer d) {
    (void)b;
    ToneState *st = get_state(d);
    push_snapshot(st);
    st->shadows = st->highlights = 0;
    st->sh_width = st->hi_width = 50;
    sh_refresh(st);
}

void image_shadows_highlights_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "tone-state", NULL);
}
static void cmd_sh_reset(GtkWidget *v) { sh_on_reset(NULL, v); }

const HelvetiaToolCommand image_shadows_highlights_commands[] = {
    { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic",
      .accel = NULL, .tooltip = "Reset", .activate = cmd_sh_reset },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

GtkWidget *image_shadows_highlights_create(void) {
    ToneState *st = g_new0(ToneState, 1);
    st->undo_stack = g_ptr_array_new();
    st->sh_width = st->hi_width = 50;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;
    image_install_edit_shortcuts(root,
        (ImageToolCallback)on_undo,
        (ImageToolCallback)sh_on_reset,
        root);

    GtkWidget *sliders;
    GtkWidget *stack = build_shell(st, &sliders, "Image file",
                                    sh_on_drop, G_CALLBACK(sh_on_save),
                                    G_CALLBACK(sh_on_reset), root);

    GtkWidget *s1, *l1, *s2, *l2, *s3, *l3, *s4, *l4;
    gtk_box_append(GTK_BOX(sliders),
        make_slider("Shadows", -100, 100, 1, 0, &s1, &l1,
                     G_CALLBACK(sh_on_shadows), root));
    gtk_box_append(GTK_BOX(sliders),
        make_slider("Shadow width", 0, 100, 1, 50, &s3, &l3,
                     G_CALLBACK(sh_on_shw), root));
    gtk_box_append(GTK_BOX(sliders),
        gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(sliders),
        make_slider("Highlights", -100, 100, 1, 0, &s2, &l2,
                     G_CALLBACK(sh_on_highlights), root));
    gtk_box_append(GTK_BOX(sliders),
        make_slider("Highlight width", 0, 100, 1, 50, &s4, &l4,
                     G_CALLBACK(sh_on_hiw), root));

    gtk_label_set_text(GTK_LABEL(l1), "+0");
    gtk_label_set_text(GTK_LABEL(l2), "+0");
    gtk_label_set_text(GTK_LABEL(l3), "50");
    gtk_label_set_text(GTK_LABEL(l4), "50");

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "tone-state", st,
                           (GDestroyNotify)tone_state_free);
    return root;
}

/* ================================================================== */
/* TOOL 2 — GAMMA CORRECTION                                          */
/* ================================================================== */

static void gm_refresh(ToneState *st) {
    g_clear_object(&st->preview);
    st->preview = apply_gamma(st);
    update_preview(st);
}

static void gm_on_gamma(GtkRange *r, gpointer d) {
    ToneState *st = get_state(d);
    double v = gtk_range_get_value(r);
    if (fabs(v - st->gamma) < 0.005) return;
    push_snapshot(st);
    st->gamma = v;
    gm_refresh(st);
}

static void gm_on_save(GtkButton *b, gpointer d) {
    (void)b; on_save_common(get_state(d), "gamma");
}
static void gm_on_drop(const char *path, gpointer d) {
    on_drop_common(get_state(d), path);
}
static void gm_on_reset(GtkButton *b, gpointer d) {
    (void)b;
    ToneState *st = get_state(d);
    push_snapshot(st);
    st->gamma = 1.0;
    g_clear_object(&st->preview);
    update_preview(st);
}

void image_gamma_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "tone-state", NULL);
}
static void cmd_gm_reset(GtkWidget *v) { gm_on_reset(NULL, v); }

const HelvetiaToolCommand image_gamma_commands[] = {
    { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic",
      .accel = NULL, .tooltip = "Reset", .activate = cmd_gm_reset },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

GtkWidget *image_gamma_create(void) {
    ToneState *st = g_new0(ToneState, 1);
    st->undo_stack = g_ptr_array_new();
    st->gamma = 1.0;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;
    image_install_edit_shortcuts(root,
        (ImageToolCallback)on_undo,
        (ImageToolCallback)gm_on_reset,
        root);

    GtkWidget *sliders;
    GtkWidget *stack = build_shell(st, &sliders, "Image file",
                                    gm_on_drop, G_CALLBACK(gm_on_save),
                                    G_CALLBACK(gm_on_reset), root);

    GtkWidget *s, *l;
    gtk_box_append(GTK_BOX(sliders),
        make_slider("Gamma", 0.1, 5.0, 0.01, 1.0, &s, &l,
                     G_CALLBACK(gm_on_gamma), root));
    gtk_label_set_text(GTK_LABEL(l), "1.00");

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "tone-state", st,
                           (GDestroyNotify)tone_state_free);
    return root;
}

/* ================================================================== */
/* TOOL 3 — AUTO ENHANCE                                              */
/* ================================================================== */

static void ae_refresh(ToneState *st) {
    g_clear_object(&st->preview);
    st->preview = apply_auto_enhance(st);
    update_preview(st);
}

static void ae_on_strength(GtkRange *r, gpointer d) {
    ToneState *st = get_state(d);
    st->ae_strength = gtk_range_get_value(r);
    ae_refresh(st);
}

static void ae_on_level(GObject *sw, GParamSpec *p, gpointer d) {
    (void)p;
    ToneState *st = get_state(d);
    st->ae_auto_level = gtk_switch_get_active(GTK_SWITCH(sw));
    ae_refresh(st);
}
static void ae_on_wb(GObject *sw, GParamSpec *p, gpointer d) {
    (void)p;
    ToneState *st = get_state(d);
    st->ae_auto_wb = gtk_switch_get_active(GTK_SWITCH(sw));
    ae_refresh(st);
}
static void ae_on_sat(GObject *sw, GParamSpec *p, gpointer d) {
    (void)p;
    ToneState *st = get_state(d);
    st->ae_auto_sat = gtk_switch_get_active(GTK_SWITCH(sw));
    ae_refresh(st);
}

static void ae_on_save(GtkButton *b, gpointer d) {
    (void)b; on_save_common(get_state(d), "enhanced");
}
static void ae_on_drop(const char *path, gpointer d) {
    ToneState *st = get_state(d);
    on_drop_common(st, path);
    if (st->original) ae_refresh(st);   /* auto-apply on load */
}
static void ae_on_reset(GtkButton *b, gpointer d) {
    (void)b;
    ToneState *st = get_state(d);
    push_snapshot(st);
    st->ae_strength = 50;
    st->ae_auto_level = st->ae_auto_wb = st->ae_auto_sat = TRUE;
    ae_refresh(st);
}

void image_auto_enhance_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "tone-state", NULL);
}
static void cmd_ae_reset(GtkWidget *v) { ae_on_reset(NULL, v); }

const HelvetiaToolCommand image_auto_enhance_commands[] = {
    { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic",
      .accel = NULL, .tooltip = "Reset", .activate = cmd_ae_reset },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

static GtkWidget *make_switch_row(const char *label, gboolean initial,
                                    GCallback cb, gpointer user_data) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *lbl = gtk_label_new(label);
    gtk_widget_set_size_request(lbl, 160, -1);
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

GtkWidget *image_auto_enhance_create(void) {
    ToneState *st = g_new0(ToneState, 1);
    st->undo_stack = g_ptr_array_new();
    st->ae_strength = 50;
    st->ae_auto_level = st->ae_auto_wb = st->ae_auto_sat = TRUE;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;
    image_install_edit_shortcuts(root,
        (ImageToolCallback)on_undo,
        (ImageToolCallback)ae_on_reset,
        root);

    GtkWidget *sliders;
    GtkWidget *stack = build_shell(st, &sliders, "Image file",
                                    ae_on_drop, G_CALLBACK(ae_on_save),
                                    G_CALLBACK(ae_on_reset), root);

    GtkWidget *s, *l;
    gtk_box_append(GTK_BOX(sliders),
        make_slider("Strength", 0, 100, 1, 50, &s, &l,
                     G_CALLBACK(ae_on_strength), root));
    gtk_label_set_text(GTK_LABEL(l), "50");

    gtk_box_append(GTK_BOX(sliders),
        gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

    gtk_box_append(GTK_BOX(sliders),
        make_switch_row("Auto level", TRUE, G_CALLBACK(ae_on_level), root));
    gtk_box_append(GTK_BOX(sliders),
        make_switch_row("Auto white balance", TRUE, G_CALLBACK(ae_on_wb), root));
    gtk_box_append(GTK_BOX(sliders),
        make_switch_row("Auto saturation", TRUE, G_CALLBACK(ae_on_sat), root));

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "tone-state", st,
                           (GDestroyNotify)tone_state_free);
    return root;
}

/* ================================================================== */
/* TOOL 4 — HISTOGRAM (viewer, no editing)                            */
/* ================================================================== */

typedef struct {
    GdkPixbuf *original;
    char      *path;
    guint      hist_r[256], hist_g[256], hist_b[256], hist_l[256];
    guint      max_r, max_g, max_b, max_l;
    int        mode;   /* 0=RGB, 1=R, 2=G, 3=B, 4=Luma */
    GtkWidget *stack, *draw_area, *stats_lbl, *root;
} HistState;

static HistState *get_hist_state(GtkWidget *v) {
    return g_object_get_data(G_OBJECT(v), "hist-state");
}

static void hist_compute(HistState *st) {
    memset(st->hist_r, 0, sizeof st->hist_r);
    memset(st->hist_g, 0, sizeof st->hist_g);
    memset(st->hist_b, 0, sizeof st->hist_b);
    memset(st->hist_l, 0, sizeof st->hist_l);
    st->max_r = st->max_g = st->max_b = st->max_l = 0;
    if (!st->original) return;

    int w = gdk_pixbuf_get_width(st->original);
    int h = gdk_pixbuf_get_height(st->original);
    int n = gdk_pixbuf_get_n_channels(st->original);
    int stride = gdk_pixbuf_get_rowstride(st->original);
    guchar *px = gdk_pixbuf_get_pixels(st->original);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            guchar *p = px + y * stride + x * n;
            st->hist_r[p[0]]++;
            st->hist_g[p[1]]++;
            st->hist_b[p[2]]++;
            double lum = 0.2126 * p[0] + 0.7152 * p[1] + 0.0722 * p[2];
            int bin = CLAMP((int)lum, 0, 255);
            st->hist_l[bin]++;
        }
    }
    for (int i = 2; i < 254; i++) {
        if (st->hist_r[i] > st->max_r) st->max_r = st->hist_r[i];
        if (st->hist_g[i] > st->max_g) st->max_g = st->hist_g[i];
        if (st->hist_b[i] > st->max_b) st->max_b = st->hist_b[i];
        if (st->hist_l[i] > st->max_l) st->max_l = st->hist_l[i];
    }
    if (!st->max_r) st->max_r = 1;
    if (!st->max_g) st->max_g = 1;
    if (!st->max_b) st->max_b = 1;
    if (!st->max_l) st->max_l = 1;
}

static void hist_draw(GtkDrawingArea *a, cairo_t *cr, int w, int h, gpointer d) {
    (void)a;
    HistState *st = d;

    cairo_set_source_rgb(cr, 0.08, 0.08, 0.08);
    cairo_paint(cr);

    if (!st->original) return;

    double bar_w = (double)w / 256.0;

    cairo_set_line_width(cr, 1.0);

    if (st->mode == 0) {
        /* RGB overlay — additive blending */
        cairo_set_operator(cr, CAIRO_OPERATOR_ADD);
        for (int i = 0; i < 256; i++) {
            double rh = (double)st->hist_r[i] / st->max_r * (h - 4);
            double gh = (double)st->hist_g[i] / st->max_g * (h - 4);
            double bh = (double)st->hist_b[i] / st->max_b * (h - 4);
            cairo_set_source_rgba(cr, 0.8, 0.1, 0.1, 0.6);
            cairo_rectangle(cr, i * bar_w, h - rh, bar_w + 0.5, rh);
            cairo_fill(cr);
            cairo_set_source_rgba(cr, 0.1, 0.8, 0.1, 0.6);
            cairo_rectangle(cr, i * bar_w, h - gh, bar_w + 0.5, gh);
            cairo_fill(cr);
            cairo_set_source_rgba(cr, 0.1, 0.2, 0.9, 0.6);
            cairo_rectangle(cr, i * bar_w, h - bh, bar_w + 0.5, bh);
            cairo_fill(cr);
        }
        cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    } else {
        guint *hist = NULL;
        guint maxv = 1;
        double rr = 1, gg = 1, bb = 1;
        if (st->mode == 1) { hist = st->hist_r; maxv = st->max_r; gg = 0.1; bb = 0.1; }
        if (st->mode == 2) { hist = st->hist_g; maxv = st->max_g; rr = 0.1; bb = 0.1; }
        if (st->mode == 3) { hist = st->hist_b; maxv = st->max_b; rr = 0.1; gg = 0.1; }
        if (st->mode == 4) { hist = st->hist_l; maxv = st->max_l; rr = gg = bb = 0.85; }

        for (int i = 0; i < 256; i++) {
            double bh = (double)hist[i] / maxv * (h - 4);
            if (bh < 0) bh = 0;
            cairo_set_source_rgba(cr, rr, gg, bb, 0.85);
            cairo_rectangle(cr, i * bar_w, h - bh, bar_w + 0.5, bh);
            cairo_fill(cr);
        }
    }

    /* Grid */
    cairo_set_source_rgba(cr, 1, 1, 1, 0.1);
    for (int i = 1; i < 4; i++) {
        cairo_move_to(cr, w * i / 4.0, 0);
        cairo_line_to(cr, w * i / 4.0, h);
    }
    cairo_stroke(cr);

    cairo_set_source_rgba(cr, 1, 1, 1, 0.15);
    cairo_rectangle(cr, 0.5, 0.5, w - 1, h - 1);
    cairo_stroke(cr);
}

static void hist_update_stats(HistState *st) {
    if (!st->original) {
        gtk_label_set_text(GTK_LABEL(st->stats_lbl), "");
        return;
    }

    guint64 sum_r = 0, sum_g = 0, sum_b = 0, sum_l = 0;
    guint64 total = 0;
    for (int i = 0; i < 256; i++) {
        sum_r += (guint64)i * st->hist_r[i];
        sum_g += (guint64)i * st->hist_g[i];
        sum_b += (guint64)i * st->hist_b[i];
        sum_l += (guint64)i * st->hist_l[i];
        total += st->hist_l[i];
    }
    if (total == 0) total = 1;

    double avg_r = (double)sum_r / total;
    double avg_g = (double)sum_g / total;
    double avg_b = (double)sum_b / total;
    double avg_l = (double)sum_l / total;

    int w = gdk_pixbuf_get_width(st->original);
    int h = gdk_pixbuf_get_height(st->original);

    char buf[256];
    snprintf(buf, sizeof buf,
             "%d × %d   ·   R̄ %.0f   Ḡ %.0f   B̄ %.0f   ·   Luma %.0f",
             w, h, avg_r, avg_g, avg_b, avg_l);
    gtk_label_set_text(GTK_LABEL(st->stats_lbl), buf);
}

static void hist_on_mode(GObject *dd, GParamSpec *p, gpointer d) {
    (void)p;
    HistState *st = get_hist_state(d);
    st->mode = gtk_drop_down_get_selected(GTK_DROP_DOWN(dd));
    gtk_widget_queue_draw(st->draw_area);
}

static void hist_on_drop(const char *path, gpointer d) {
    HistState *st = get_hist_state(d);
    GError *e = NULL;
    GdkPixbuf *pb = gdk_pixbuf_new_from_file(path, &e);
    if (!pb) { image_show_error(st->root, e->message); g_error_free(e); return; }
    g_clear_object(&st->original);
    g_free(st->path);
    st->original = pb;
    st->path = g_strdup(path);
    hist_compute(st);
    hist_update_stats(st);
    gtk_widget_queue_draw(st->draw_area);
    gtk_stack_set_visible_child_name(GTK_STACK(st->stack), "viewer");
}

void image_histogram_on_close(GtkWidget *v) {
    HistState *st = get_hist_state(v);
    if (st) {
        g_clear_object(&st->original);
        g_free(st->path);
        g_free(st);
    }
    g_object_set_data(G_OBJECT(v), "hist-state", NULL);
}

const HelvetiaToolCommand image_histogram_commands[] = {
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

GtkWidget *image_histogram_create(void) {
    HistState *st = g_new0(HistState, 1);
    st->mode = 0;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;

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
        image_build_drop_zone("Image file", hist_on_drop, root));
    gtk_stack_add_named(GTK_STACK(stack), drop_box, "drop");

    /* Viewer page */
    GtkWidget *viewer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(bar, 12);
    gtk_widget_set_margin_end(bar, 12);
    gtk_widget_set_margin_top(bar, 8);
    gtk_widget_set_margin_bottom(bar, 8);

    GtkWidget *mode_lbl = gtk_label_new("Channel:");
    gtk_widget_add_css_class(mode_lbl, "dim-label");
    const char *modes[] = {"RGB", "Red", "Green", "Blue", "Luma", NULL};
    GtkWidget *mode_dd = gtk_drop_down_new_from_strings(modes);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(mode_dd), 0);
    g_signal_connect(mode_dd, "notify::selected",
                     G_CALLBACK(hist_on_mode), root);

    GtkWidget *sp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(sp, TRUE);

    gtk_box_append(GTK_BOX(bar), mode_lbl);
    gtk_box_append(GTK_BOX(bar), mode_dd);
    gtk_box_append(GTK_BOX(bar), gtk_separator_new(GTK_ORIENTATION_VERTICAL));
    gtk_box_append(GTK_BOX(bar), image_new_image_button(hist_on_drop, root));
    gtk_box_append(GTK_BOX(bar), sp);

    GtkWidget *draw = gtk_drawing_area_new();
    gtk_widget_set_size_request(draw, -1, 260);
    gtk_widget_set_vexpand(draw, TRUE);
    gtk_widget_set_margin_start(draw, 12);
    gtk_widget_set_margin_end(draw, 12);
    gtk_widget_set_margin_bottom(draw, 8);
    st->draw_area = draw;
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(draw), hist_draw, st, NULL);

    GtkWidget *stats = gtk_label_new("");
    gtk_widget_add_css_class(stats, "dim-label");
    gtk_widget_add_css_class(stats, "caption");
    gtk_widget_set_margin_start(stats, 12);
    gtk_widget_set_margin_bottom(stats, 12);
    gtk_label_set_xalign(GTK_LABEL(stats), 0.0f);
    st->stats_lbl = stats;

    gtk_box_append(GTK_BOX(viewer), bar);
    gtk_box_append(GTK_BOX(viewer),
                   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(viewer), draw);
    gtk_box_append(GTK_BOX(viewer), stats);
    gtk_stack_add_named(GTK_STACK(stack), viewer, "viewer");
    gtk_stack_set_visible_child_name(GTK_STACK(stack), "drop");

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data(G_OBJECT(root), "hist-state", st);
    return root;
}
