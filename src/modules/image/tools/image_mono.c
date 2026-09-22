#include <gtk/gtk.h>
#include <adwaita.h>
#include <math.h>
#include "../image_shared.h"
#include "image_mono.h"

/* ================================================================== */
/* SHARED STATE                                                       */
/* ================================================================== */

typedef struct {
    GdkPixbuf *original;
    GdkPixbuf *preview;
    GdkPixbuf *first_original;
    GPtrArray *undo_stack;
    char      *path;

    /* Sepia */
    double     sepia_intensity;   /* 0..100 */

    /* Grayscale */
    double     gray_strength;     /* 0..100 */
    double     gray_r_weight;     /* 0..100 */
    double     gray_g_weight;     /* 0..100 */
    double     gray_b_weight;     /* 0..100 */

    double     zoom;
    GtkWidget *stack, *picture, *root;
    GtkWidget *undo_btn;
} MonoState;

static MonoState *get_state(GtkWidget *v) {
    return g_object_get_data(G_OBJECT(v), "mono-state");
}

/* ================================================================== */
/* UNDO                                                               */
/* ================================================================== */

static void push_snapshot(MonoState *st) {
    double *s = g_new0(double, 5);
    s[0] = st->sepia_intensity;
    s[1] = st->gray_strength;
    s[2] = st->gray_r_weight;
    s[3] = st->gray_g_weight;
    s[4] = st->gray_b_weight;
    g_ptr_array_add(st->undo_stack, s);
    gtk_widget_set_sensitive(st->undo_btn, TRUE);
}

static void on_undo(GtkButton *b, gpointer d) {
    (void)b;
    MonoState *st = get_state(d);
    if (st->undo_stack->len == 0) return;
    double *s = g_ptr_array_index(st->undo_stack, st->undo_stack->len - 1);
    g_ptr_array_remove_index(st->undo_stack, st->undo_stack->len - 1);
    st->sepia_intensity = s[0];
    st->gray_strength = s[1];
    st->gray_r_weight = s[2];
    st->gray_g_weight = s[3];
    st->gray_b_weight = s[4];
    g_free(s);
    gtk_widget_set_sensitive(st->undo_btn, st->undo_stack->len > 0);
}

static void clear_undo(MonoState *st) {
    for (guint i = 0; i < st->undo_stack->len; i++)
        g_free(g_ptr_array_index(st->undo_stack, i));
    g_ptr_array_set_size(st->undo_stack, 0);
    gtk_widget_set_sensitive(st->undo_btn, FALSE);
}

/* ================================================================== */
/* SHARED HELPERS                                                     */
/* ================================================================== */

static void update_preview(MonoState *st) {
    GdkPixbuf *src = st->preview ? st->preview : st->original;
    if (!src) return;
    gtk_picture_set_paintable(GTK_PICTURE(st->picture), GDK_PAINTABLE(gdk_texture_new_for_pixbuf(src)));
    gtk_picture_set_content_fit(GTK_PICTURE(st->picture),
                                 GTK_CONTENT_FIT_CONTAIN);
    gtk_picture_set_can_shrink(GTK_PICTURE(st->picture), TRUE);
    
}

static void on_save_common(MonoState *st, const char *prefix) {
    GdkPixbuf *src = st->preview ? st->preview : st->original;
    if (!src) return;
    char *base = st->path ? g_path_get_basename(st->path) : g_strdup("image.png");
    char *name = g_strdup_printf("%s_%s", prefix, base);
    image_save_pixbuf_dialog(st->root, src, name);
    g_free(name);
    g_free(base);
}

static void on_drop_common(MonoState *st, const char *path) {
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

    st->sepia_intensity = 100;
    st->gray_strength = 100;
    st->gray_r_weight = 21.26;
    st->gray_g_weight = 71.52;
    st->gray_b_weight = 7.22;

    update_preview(st);
    gtk_stack_set_visible_child_name(GTK_STACK(st->stack), "editor");
}

static void mono_state_free(MonoState *st) {
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
/* SEPIA                                                              */
/* ================================================================== */

/*
 * Classic sepia formula (per-channel linear transform):
 *   R' = 0.393*R + 0.769*G + 0.189*B
 *   G' = 0.349*R + 0.686*G + 0.168*B
 *   B' = 0.272*R + 0.534*G + 0.131*B
 * Intensity blends between the original and the sepia result.
 */

static GdkPixbuf *apply_sepia(MonoState *st) {
    GdkPixbuf *src = st->original;
    int w = gdk_pixbuf_get_width(src);
    int h = gdk_pixbuf_get_height(src);
    int n = gdk_pixbuf_get_n_channels(src);
    int stride = gdk_pixbuf_get_rowstride(src);
    guchar *px = gdk_pixbuf_get_pixels(src);

    double strength = st->sepia_intensity / 100.0;

    GdkPixbuf *out = gdk_pixbuf_copy(src);
    int ostride = gdk_pixbuf_get_rowstride(out);
    guchar *opx = gdk_pixbuf_get_pixels(out);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            guchar *sp = px + y * stride + x * n;
            guchar *dp = opx + y * ostride + x * n;

            double r = sp[0], g = sp[1], b = sp[2];

            double sr = 0.393 * r + 0.769 * g + 0.189 * b;
            double sg = 0.349 * r + 0.686 * g + 0.168 * b;
            double sb = 0.272 * r + 0.534 * g + 0.131 * b;

            /* Blend with original based on strength */
            sr = r + (sr - r) * strength;
            sg = g + (sg - g) * strength;
            sb = b + (sb - b) * strength;

            dp[0] = (guchar)clampd(sr + 0.5, 0, 255);
            dp[1] = (guchar)clampd(sg + 0.5, 0, 255);
            dp[2] = (guchar)clampd(sb + 0.5, 0, 255);
            if (n == 4) dp[3] = sp[3];
        }
    }
    return out;
}

/* ================================================================== */
/* GRAYSCALE                                                          */
/* ================================================================== */

static GdkPixbuf *apply_grayscale(MonoState *st) {
    GdkPixbuf *src = st->original;
    int w = gdk_pixbuf_get_width(src);
    int h = gdk_pixbuf_get_height(src);
    int n = gdk_pixbuf_get_n_channels(src);
    int stride = gdk_pixbuf_get_rowstride(src);
    guchar *px = gdk_pixbuf_get_pixels(src);

    double strength = st->gray_strength / 100.0;
    /* Normalize weights so total = 1.0 */
    double wr = st->gray_r_weight;
    double wg = st->gray_g_weight;
    double wb = st->gray_b_weight;
    double wsum = wr + wg + wb;
    if (wsum < 0.001) { wr = 0.2126; wg = 0.7152; wb = 0.0722; wsum = 1.0; }
    wr /= wsum; wg /= wsum; wb /= wsum;

    GdkPixbuf *out = gdk_pixbuf_copy(src);
    int ostride = gdk_pixbuf_get_rowstride(out);
    guchar *opx = gdk_pixbuf_get_pixels(out);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            guchar *sp = px + y * stride + x * n;
            guchar *dp = opx + y * ostride + x * n;

            double r = sp[0], g = sp[1], b = sp[2];
            double gray = wr * r + wg * g + wb * b;

            /* Blend toward grayscale based on strength */
            dp[0] = (guchar)clampd(r + (gray - r) * strength + 0.5, 0, 255);
            dp[1] = (guchar)clampd(g + (gray - g) * strength + 0.5, 0, 255);
            dp[2] = (guchar)clampd(b + (gray - b) * strength + 0.5, 0, 255);
            if (n == 4) dp[3] = sp[3];
        }
    }
    return out;
}

/* ================================================================== */
/* INVERT                                                             */
/* ================================================================== */

static GdkPixbuf *apply_invert(MonoState *st) {
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
            dp[0] = 255 - sp[0];
            dp[1] = 255 - sp[1];
            dp[2] = 255 - sp[2];
            if (n == 4) dp[3] = sp[3];   /* keep alpha */
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

static GtkWidget *build_shell(MonoState *st, GtkWidget **out_sliders,
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
/* TOOL 1 — SEPIA                                                     */
/* ================================================================== */

static void sp_refresh(MonoState *st) {
    g_clear_object(&st->preview);
    st->preview = apply_sepia(st);
    update_preview(st);
}

static void sp_on_intensity(GtkRange *r, gpointer d) {
    MonoState *st = get_state(d);
    double v = gtk_range_get_value(r);
    if (fabs(v - st->sepia_intensity) < 0.5) return;
    push_snapshot(st);
    st->sepia_intensity = v;
    sp_refresh(st);
}

static void sp_on_save(GtkButton *b, gpointer d) {
    (void)b; on_save_common(get_state(d), "sepia");
}
static void sp_on_drop(const char *path, gpointer d) {
    MonoState *st = get_state(d);
    on_drop_common(st, path);
    if (st->original) sp_refresh(st);
}
static void sp_on_reset(GtkButton *b, gpointer d) {
    (void)b;
    MonoState *st = get_state(d);
    push_snapshot(st);
    st->sepia_intensity = 100;
    sp_refresh(st);
}

void image_sepia_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "mono-state", NULL);
}
static void cmd_sp_reset(GtkWidget *v) { sp_on_reset(NULL, v); }

const HelvetiaToolCommand image_sepia_commands[] = {
    { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic",
      .accel = NULL, .tooltip = "Reset", .activate = cmd_sp_reset },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

GtkWidget *image_sepia_create(void) {
    MonoState *st = g_new0(MonoState, 1);
    st->undo_stack = g_ptr_array_new();
    st->zoom = 1.0;
    st->sepia_intensity = 100;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;

    GtkWidget *sliders;
    GtkWidget *stack = build_shell(st, &sliders, "Image file",
                                    sp_on_drop, G_CALLBACK(sp_on_save),
                                    G_CALLBACK(sp_on_reset), root);

    GtkWidget *s, *l;
    gtk_box_append(GTK_BOX(sliders),
        make_slider("Intensity", 0, 100, 1, 100, &s, &l,
                     G_CALLBACK(sp_on_intensity), root));
    gtk_label_set_text(GTK_LABEL(l), "100");

    GtkWidget *hint = gtk_label_new(
        "Applies the classic sepia tone. "
        "Lower intensity blends toward the original.");
    gtk_label_set_wrap(GTK_LABEL(hint), TRUE);
    gtk_widget_add_css_class(hint, "dim-label");
    gtk_widget_add_css_class(hint, "caption");
    gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);
    gtk_box_append(GTK_BOX(sliders), hint);

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "mono-state", st,
                           (GDestroyNotify)mono_state_free);
    image_register_zoom(root, st->picture, &st->zoom);
    image_install_zoom_shortcuts(root);
    return root;
}

/* ================================================================== */
/* TOOL 2 — GRAYSCALE                                                 */
/* ================================================================== */

static void gs_refresh(MonoState *st) {
    g_clear_object(&st->preview);
    st->preview = apply_grayscale(st);
    update_preview(st);
}

static void gs_on_strength(GtkRange *r, gpointer d) {
    MonoState *st = get_state(d);
    double v = gtk_range_get_value(r);
    if (fabs(v - st->gray_strength) < 0.5) return;
    push_snapshot(st);
    st->gray_strength = v;
    gs_refresh(st);
}

static void gs_on_r(GtkRange *r, gpointer d) {
    MonoState *st = get_state(d);
    st->gray_r_weight = gtk_range_get_value(r);
    gs_refresh(st);
}
static void gs_on_g(GtkRange *r, gpointer d) {
    MonoState *st = get_state(d);
    st->gray_g_weight = gtk_range_get_value(r);
    gs_refresh(st);
}
static void gs_on_b(GtkRange *r, gpointer d) {
    MonoState *st = get_state(d);
    st->gray_b_weight = gtk_range_get_value(r);
    gs_refresh(st);
}

static void gs_on_save(GtkButton *b, gpointer d) {
    (void)b; on_save_common(get_state(d), "gray");
}
static void gs_on_drop(const char *path, gpointer d) {
    MonoState *st = get_state(d);
    on_drop_common(st, path);
    if (st->original) gs_refresh(st);
}
static void gs_on_reset(GtkButton *b, gpointer d) {
    (void)b;
    MonoState *st = get_state(d);
    push_snapshot(st);
    st->gray_strength = 100;
    st->gray_r_weight = 21.26;
    st->gray_g_weight = 71.52;
    st->gray_b_weight = 7.22;
    gs_refresh(st);
}

void image_grayscale_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "mono-state", NULL);
}
static void cmd_gs_reset(GtkWidget *v) { gs_on_reset(NULL, v); }

const HelvetiaToolCommand image_grayscale_commands[] = {
    { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic",
      .accel = NULL, .tooltip = "Reset", .activate = cmd_gs_reset },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

GtkWidget *image_grayscale_create(void) {
    MonoState *st = g_new0(MonoState, 1);
    st->undo_stack = g_ptr_array_new();
    st->zoom = 1.0;
    st->gray_strength = 100;
    st->gray_r_weight = 21.26;
    st->gray_g_weight = 71.52;
    st->gray_b_weight = 7.22;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;

    GtkWidget *sliders;
    GtkWidget *stack = build_shell(st, &sliders, "Image file",
                                    gs_on_drop, G_CALLBACK(gs_on_save),
                                    G_CALLBACK(gs_on_reset), root);

    GtkWidget *s1, *l1, *s2, *l2, *s3, *l3, *s4, *l4;
    gtk_box_append(GTK_BOX(sliders),
        make_slider("Strength", 0, 100, 1, 100, &s1, &l1,
                     G_CALLBACK(gs_on_strength), root));
    gtk_label_set_text(GTK_LABEL(l1), "100");

    GtkWidget *sep = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(sep, -1, 6);
    gtk_box_append(GTK_BOX(sliders), sep);

    GtkWidget *w_lbl = gtk_label_new("Channel weights");
    gtk_widget_add_css_class(w_lbl, "heading");
    gtk_label_set_xalign(GTK_LABEL(w_lbl), 0.0f);
    gtk_box_append(GTK_BOX(sliders), w_lbl);

    gtk_box_append(GTK_BOX(sliders),
        make_slider("Red", 0, 100, 0.01, 21.26, &s2, &l2,
                     G_CALLBACK(gs_on_r), root));
    gtk_box_append(GTK_BOX(sliders),
        make_slider("Green", 0, 100, 0.01, 71.52, &s3, &l3,
                     G_CALLBACK(gs_on_g), root));
    gtk_box_append(GTK_BOX(sliders),
        make_slider("Blue", 0, 100, 0.01, 7.22, &s4, &l4,
                     G_CALLBACK(gs_on_b), root));
    gtk_label_set_text(GTK_LABEL(l2), "21.26");
    gtk_label_set_text(GTK_LABEL(l3), "71.52");
    gtk_label_set_text(GTK_LABEL(l4), "7.22");

    GtkWidget *hint = gtk_label_new(
        "Default weights match Rec.709 luma (perceptual brightness). "
        "Adjust to emphasize different color contributions.");
    gtk_label_set_wrap(GTK_LABEL(hint), TRUE);
    gtk_widget_add_css_class(hint, "dim-label");
    gtk_widget_add_css_class(hint, "caption");
    gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);
    gtk_box_append(GTK_BOX(sliders), hint);

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "mono-state", st,
                           (GDestroyNotify)mono_state_free);
    image_register_zoom(root, st->picture, &st->zoom);
    image_install_zoom_shortcuts(root);
    return root;
}

/* ================================================================== */
/* TOOL 3 — INVERT                                                    */
/* ================================================================== */

static void iv_refresh(MonoState *st) {
    g_clear_object(&st->preview);
    st->preview = apply_invert(st);
    update_preview(st);
}

static void iv_on_save(GtkButton *b, gpointer d) {
    (void)b; on_save_common(get_state(d), "inverted");
}
static void iv_on_drop(const char *path, gpointer d) {
    MonoState *st = get_state(d);
    on_drop_common(st, path);
    if (st->original) iv_refresh(st);   /* auto-apply */
}
static void iv_on_reset(GtkButton *b, gpointer d) {
    (void)b;
    MonoState *st = get_state(d);
    push_snapshot(st);
    iv_refresh(st);   /* re-invert from original = neutral */
    /* Actually reset means "show original" — clear preview */
    g_clear_object(&st->preview);
    update_preview(st);
}

void image_invert_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "mono-state", NULL);
}
static void cmd_iv_reset(GtkWidget *v) { iv_on_reset(NULL, v); }

const HelvetiaToolCommand image_invert_commands[] = {
    { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic",
      .accel = NULL, .tooltip = "Reset", .activate = cmd_iv_reset },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

GtkWidget *image_invert_create(void) {
    MonoState *st = g_new0(MonoState, 1);
    st->undo_stack = g_ptr_array_new();
    st->zoom = 1.0;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;

    GtkWidget *sliders;
    GtkWidget *stack = build_shell(st, &sliders, "Image file",
                                    iv_on_drop, G_CALLBACK(iv_on_save),
                                    G_CALLBACK(iv_on_reset), root);

    GtkWidget *hint = gtk_label_new(
        "Inverts all colors — produces a photographic negative. "
        "Applied automatically on load.");
    gtk_label_set_wrap(GTK_LABEL(hint), TRUE);
    gtk_widget_add_css_class(hint, "dim-label");
    gtk_widget_add_css_class(hint, "caption");
    gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);
    gtk_box_append(GTK_BOX(sliders), hint);

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "mono-state", st,
                           (GDestroyNotify)mono_state_free);
    image_register_zoom(root, st->picture, &st->zoom);
    image_install_zoom_shortcuts(root);
    return root;
}
