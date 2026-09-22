#include <gtk/gtk.h>
#include <adwaita.h>
#include <math.h>
#include <stdlib.h>
#include "../image_shared.h"
#include "image_denoise.h"

/* ================================================================== */
/* SHARED STATE                                                       */
/* ================================================================== */

typedef struct {
    GdkPixbuf *original;
    GdkPixbuf *preview;
    GdkPixbuf *first_original;
    GPtrArray *undo_stack;
    char      *path;

    /* Noise Reduction (median) */
    int        nr_radius;      /* 1..5 */

    /* Denoise (bilateral) */
    double     dn_strength;    /* 0..100 — spatial sigma */
    double     dn_range;       /* 0..200 — range sigma */
    int        dn_radius;      /* 1..5 — neighborhood radius */

    double     zoom;
    GtkWidget *stack, *picture, *root;
    GtkWidget *undo_btn;
} DenoiseState;

static DenoiseState *get_state(GtkWidget *v) {
    return g_object_get_data(G_OBJECT(v), "denoise-state");
}

/* ================================================================== */
/* UNDO                                                               */
/* ================================================================== */

static void push_snapshot(DenoiseState *st) {
    double *s = g_new0(double, 3);
    s[0] = (double)st->nr_radius;
    s[1] = st->dn_strength;
    s[2] = st->dn_range;
    g_ptr_array_add(st->undo_stack, s);
    gtk_widget_set_sensitive(st->undo_btn, TRUE);
}

static void on_undo(GtkButton *b, gpointer d) {
    (void)b;
    DenoiseState *st = get_state(d);
    if (st->undo_stack->len == 0) return;
    double *s = g_ptr_array_index(st->undo_stack, st->undo_stack->len - 1);
    g_ptr_array_remove_index(st->undo_stack, st->undo_stack->len - 1);
    st->nr_radius = (int)s[0];
    st->dn_strength = s[1];
    st->dn_range = s[2];
    g_free(s);
    gtk_widget_set_sensitive(st->undo_btn, st->undo_stack->len > 0);
}

static void clear_undo(DenoiseState *st) {
    for (guint i = 0; i < st->undo_stack->len; i++)
        g_free(g_ptr_array_index(st->undo_stack, i));
    g_ptr_array_set_size(st->undo_stack, 0);
    gtk_widget_set_sensitive(st->undo_btn, FALSE);
}

/* ================================================================== */
/* SHARED HELPERS                                                     */
/* ================================================================== */

static void update_preview(DenoiseState *st) {
    GdkPixbuf *src = st->preview ? st->preview : st->original;
    if (!src) return;
    gtk_picture_set_paintable(GTK_PICTURE(st->picture), GDK_PAINTABLE(gdk_texture_new_for_pixbuf(src)));
    gtk_picture_set_content_fit(GTK_PICTURE(st->picture),
                                 GTK_CONTENT_FIT_CONTAIN);
    gtk_picture_set_can_shrink(GTK_PICTURE(st->picture), TRUE);
    
}

static void on_save_common(DenoiseState *st, const char *prefix) {
    GdkPixbuf *src = st->preview ? st->preview : st->original;
    if (!src) return;
    char *base = st->path ? g_path_get_basename(st->path) : g_strdup("image.png");
    char *name = g_strdup_printf("%s_%s", prefix, base);
    image_save_pixbuf_dialog(st->root, src, name);
    g_free(name);
    g_free(base);
}

static void on_drop_common(DenoiseState *st, const char *path) {
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

    st->nr_radius = 2;
    st->dn_strength = 20;
    st->dn_range = 30;
    st->dn_radius = 3;

    update_preview(st);
    gtk_stack_set_visible_child_name(GTK_STACK(st->stack), "editor");
}

static void denoise_state_free(DenoiseState *st) {
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
/* NOISE REDUCTION — MEDIAN FILTER                                    */
/* ================================================================== */

/*
 * Median filter: for each pixel, collect all neighbors in a (2r+1)²
 * window, sort per channel, take the median. Excellent at removing
 * salt-and-pepper noise while preserving edges.
 */

static int compare_u8(const void *a, const void *b) {
    return (int)(*(const guchar *)a) - (int)(*(const guchar *)b);
}

static GdkPixbuf *apply_noise_reduction(DenoiseState *st) {
    GdkPixbuf *src = st->original;
    int w = gdk_pixbuf_get_width(src);
    int h = gdk_pixbuf_get_height(src);
    int n = gdk_pixbuf_get_n_channels(src);
    int stride = gdk_pixbuf_get_rowstride(src);
    guchar *px = gdk_pixbuf_get_pixels(src);

    int r = st->nr_radius;
    if (r < 1) return g_object_ref(src);
    if (r > 5) r = 5;
    int ksize = (2 * r + 1) * (2 * r + 1);

    GdkPixbuf *out = gdk_pixbuf_copy(src);
    int ostride = gdk_pixbuf_get_rowstride(out);
    guchar *opx = gdk_pixbuf_get_pixels(out);

    guchar *window = g_new(guchar, ksize);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            /* Copy edges unchanged (or clamp-window inside) */
            if (x < r || x >= w - r || y < r || y >= h - r) {
                for (int c = 0; c < n; c++)
                    opx[y * ostride + x * n + c] =
                        px[y * stride + x * n + c];
                continue;
            }

            for (int c = 0; c < 3; c++) {
                int k = 0;
                for (int dy = -r; dy <= r; dy++)
                    for (int dx = -r; dx <= r; dx++)
                        window[k++] = px[(y + dy) * stride + (x + dx) * n + c];

                qsort(window, ksize, sizeof(guchar), compare_u8);
                opx[y * ostride + x * n + c] = window[ksize / 2];
            }
            if (n == 4)
                opx[y * ostride + x * n + 3] = px[y * stride + x * n + 3];
        }
    }

    g_free(window);
    return out;
}

/* ================================================================== */
/* DENOISE — BILATERAL FILTER                                         */
/* ================================================================== */

/*
 * Bilateral filter: for each pixel, average neighbors weighted by
 *   spatial_weight  = exp(-d² / (2 * σ_spatial²))
 *   range_weight    = exp(-Δcolor² / (2 * σ_range²))
 * This smooths flat areas but preserves edges where color changes sharply.
 */

static GdkPixbuf *apply_denoise(DenoiseState *st) {
    GdkPixbuf *src = st->original;
    int w = gdk_pixbuf_get_width(src);
    int h = gdk_pixbuf_get_height(src);
    int n = gdk_pixbuf_get_n_channels(src);
    int stride = gdk_pixbuf_get_rowstride(src);
    guchar *px = gdk_pixbuf_get_pixels(src);

    int r = st->dn_radius;
    if (r < 1) r = 1;
    if (r > 5) r = 5;

    /* Spatial sigma from strength (0..100 → 1..6 px) */
    double sigma_s = 1.0 + st->dn_strength / 20.0;   /* 1..6 */
    double inv_2ss2 = 1.0 / (2.0 * sigma_s * sigma_s);

    /* Range sigma from range (0..200 → 5..100 intensity) */
    double sigma_r = 5.0 + st->dn_range * 0.5;      /* 5..105 */
    double inv_2sr2 = 1.0 / (2.0 * sigma_r * sigma_r);

    /* Precompute spatial weights */
    double *spatial_w = g_new(double, (2*r+1) * (2*r+1));
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++)
            spatial_w[(dy + r) * (2*r+1) + (dx + r)] =
                exp(-(dx*dx + dy*dy) * inv_2ss2);

    GdkPixbuf *out = gdk_pixbuf_copy(src);
    int ostride = gdk_pixbuf_get_rowstride(out);
    guchar *opx = gdk_pixbuf_get_pixels(out);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            if (x < r || x >= w - r || y < r || y >= h - r) {
                for (int c = 0; c < n; c++)
                    opx[y * ostride + x * n + c] =
                        px[y * stride + x * n + c];
                continue;
            }

            guchar center[4] = {0};
            for (int c = 0; c < 3; c++)
                center[c] = px[y * stride + x * n + c];

            double sum[3] = {0, 0, 0};
            double weight_sum = 0;

            for (int dy = -r; dy <= r; dy++) {
                for (int dx = -r; dx <= r; dx++) {
                    guchar *np = px + (y + dy) * stride + (x + dx) * n;

                    /* Color distance squared */
                    double dr = (double)np[0] - center[0];
                    double dg = (double)np[1] - center[1];
                    double db = (double)np[2] - center[2];
                    double color_d2 = dr*dr + dg*dg + db*db;

                    double range_w = exp(-color_d2 * inv_2sr2);
                    double sw = spatial_w[(dy + r) * (2*r+1) + (dx + r)];
                    double w = sw * range_w;

                    sum[0] += w * np[0];
                    sum[1] += w * np[1];
                    sum[2] += w * np[2];
                    weight_sum += w;
                }
            }

            if (weight_sum < 1e-9) weight_sum = 1e-9;
            for (int c = 0; c < 3; c++)
                opx[y * ostride + x * n + c] =
                    (guchar)clampd(sum[c] / weight_sum + 0.5, 0, 255);
            if (n == 4)
                opx[y * ostride + x * n + 3] = px[y * stride + x * n + 3];
        }
    }

    g_free(spatial_w);
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

static GtkWidget *build_shell(DenoiseState *st, GtkWidget **out_sliders,
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
/* TOOL 1 — NOISE REDUCTION (MEDIAN)                                  */
/* ================================================================== */

static void nr_refresh(DenoiseState *st) {
    g_clear_object(&st->preview);
    st->preview = apply_noise_reduction(st);
    update_preview(st);
}

static void nr_on_radius(GtkRange *r, gpointer d) {
    DenoiseState *st = get_state(d);
    int v = (int)gtk_range_get_value(r);
    if (v == st->nr_radius) return;
    push_snapshot(st);
    st->nr_radius = v;
    nr_refresh(st);
}

static void nr_on_save(GtkButton *b, gpointer d) {
    (void)b; on_save_common(get_state(d), "noisereduced");
}
static void nr_on_drop(const char *path, gpointer d) {
    DenoiseState *st = get_state(d);
    on_drop_common(st, path);
    if (st->original) nr_refresh(st);
}
static void nr_on_reset(GtkButton *b, gpointer d) {
    (void)b;
    DenoiseState *st = get_state(d);
    push_snapshot(st);
    st->nr_radius = 2;
    nr_refresh(st);
}

void image_noise_reduction_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "denoise-state", NULL);
}
static void cmd_nr_reset(GtkWidget *v) { nr_on_reset(NULL, v); }

const HelvetiaToolCommand image_noise_reduction_commands[] = {
    { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic",
      .accel = NULL, .tooltip = "Reset", .activate = cmd_nr_reset },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

GtkWidget *image_noise_reduction_create(void) {
    DenoiseState *st = g_new0(DenoiseState, 1);
    st->undo_stack = g_ptr_array_new();
    st->zoom = 1.0;
    st->nr_radius = 2;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;

    GtkWidget *sliders;
    GtkWidget *stack = build_shell(st, &sliders, "Image file",
                                    nr_on_drop, G_CALLBACK(nr_on_save),
                                    G_CALLBACK(nr_on_reset), root);

    GtkWidget *s, *l;
    gtk_box_append(GTK_BOX(sliders),
        make_slider("Radius", 1, 5, 1, 2, &s, &l,
                     G_CALLBACK(nr_on_radius), root));
    gtk_label_set_text(GTK_LABEL(l), "2");

    /* Hint */
    GtkWidget *hint = gtk_label_new(
        "Median filter. Best for salt-and-pepper noise. "
        "Larger radius removes more noise but softens detail.");
    gtk_label_set_wrap(GTK_LABEL(hint), TRUE);
    gtk_widget_add_css_class(hint, "dim-label");
    gtk_widget_add_css_class(hint, "caption");
    gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);
    gtk_box_append(GTK_BOX(sliders), hint);

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "denoise-state", st,
                           (GDestroyNotify)denoise_state_free);
    image_register_zoom(root, st->picture, &st->zoom);
    image_install_zoom_shortcuts(root);
    return root;
}

/* ================================================================== */
/* TOOL 2 — DENOISE (BILATERAL)                                       */
/* ================================================================== */

static void dn_refresh(DenoiseState *st) {
    g_clear_object(&st->preview);
    st->preview = apply_denoise(st);
    update_preview(st);
}

static void dn_on_strength(GtkRange *r, gpointer d) {
    DenoiseState *st = get_state(d);
    double v = gtk_range_get_value(r);
    if (fabs(v - st->dn_strength) < 0.5) return;
    push_snapshot(st);
    st->dn_strength = v;
    dn_refresh(st);
}

static void dn_on_range(GtkRange *r, gpointer d) {
    DenoiseState *st = get_state(d);
    st->dn_range = gtk_range_get_value(r);
    dn_refresh(st);
}

static void dn_on_radius(GtkRange *r, gpointer d) {
    DenoiseState *st = get_state(d);
    int v = (int)gtk_range_get_value(r);
    if (v == st->dn_radius) return;
    st->dn_radius = v;
    dn_refresh(st);
}

static void dn_on_save(GtkButton *b, gpointer d) {
    (void)b; on_save_common(get_state(d), "denoised");
}
static void dn_on_drop(const char *path, gpointer d) {
    DenoiseState *st = get_state(d);
    on_drop_common(st, path);
    if (st->original) dn_refresh(st);
}
static void dn_on_reset(GtkButton *b, gpointer d) {
    (void)b;
    DenoiseState *st = get_state(d);
    push_snapshot(st);
    st->dn_strength = 20;
    st->dn_range = 30;
    st->dn_radius = 3;
    dn_refresh(st);
}

void image_denoise_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "denoise-state", NULL);
}
static void cmd_dn_reset(GtkWidget *v) { dn_on_reset(NULL, v); }

const HelvetiaToolCommand image_denoise_commands[] = {
    { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic",
      .accel = NULL, .tooltip = "Reset", .activate = cmd_dn_reset },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

GtkWidget *image_denoise_create(void) {
    DenoiseState *st = g_new0(DenoiseState, 1);
    st->undo_stack = g_ptr_array_new();
    st->zoom = 1.0;
    st->dn_strength = 20;
    st->dn_range = 30;
    st->dn_radius = 3;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;

    GtkWidget *sliders;
    GtkWidget *stack = build_shell(st, &sliders, "Image file",
                                    dn_on_drop, G_CALLBACK(dn_on_save),
                                    G_CALLBACK(dn_on_reset), root);

    GtkWidget *s1, *l1, *s2, *l2, *s3, *l3;
    gtk_box_append(GTK_BOX(sliders),
        make_slider("Strength", 0, 100, 1, 20, &s1, &l1,
                     G_CALLBACK(dn_on_strength), root));
    gtk_box_append(GTK_BOX(sliders),
        make_slider("Radius", 1, 5, 1, 3, &s3, &l3,
                     G_CALLBACK(dn_on_radius), root));
    gtk_box_append(GTK_BOX(sliders),
        make_slider("Edge keep", 0, 200, 1, 30, &s2, &l2,
                     G_CALLBACK(dn_on_range), root));
    gtk_label_set_text(GTK_LABEL(l1), "20");
    gtk_label_set_text(GTK_LABEL(l2), "30");
    gtk_label_set_text(GTK_LABEL(l3), "3");

    GtkWidget *hint = gtk_label_new(
        "Bilateral filter. Smooths flat areas while preserving edges. "
        "Higher 'Edge keep' preserves more detail at edges.");
    gtk_label_set_wrap(GTK_LABEL(hint), TRUE);
    gtk_widget_add_css_class(hint, "dim-label");
    gtk_widget_add_css_class(hint, "caption");
    gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);
    gtk_box_append(GTK_BOX(sliders), hint);

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "denoise-state", st,
                           (GDestroyNotify)denoise_state_free);
    image_register_zoom(root, st->picture, &st->zoom);
    image_install_zoom_shortcuts(root);
    return root;
}
