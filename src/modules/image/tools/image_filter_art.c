#include <gtk/gtk.h>
#include <adwaita.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "../image_shared.h"
#include "image_filter_art.h"

/* ================================================================== */
/* COMMON STATE                                                       */
/* ================================================================== */

typedef struct {
    GdkPixbuf *original;
    GdkPixbuf *preview;
    GPtrArray *undo_stack;
    char      *path;

    double     val1; /* Vignette strength / Grain intensity / Glow strength */
    double     val2; /* Vignette radius / Glow radius */

    GtkWidget *stack, *picture, *root;
    GtkWidget *undo_btn;
} FilterState;

static FilterState *get_state(GtkWidget *v) {
    return g_object_get_data(G_OBJECT(v), "filter-state");
}

static void push_snapshot(FilterState *st) {
    image_undo_push(st->undo_stack, st->original);
    gtk_widget_set_sensitive(st->undo_btn, TRUE);
}

static void on_undo(GtkButton *b, gpointer d) {
    (void)b; FilterState *st = get_state(d);
    if (!st->undo_stack || st->undo_stack->len == 0) return;
    GdkPixbuf *prev = image_undo_pop(st->undo_stack);
    if (prev) {
        g_clear_object(&st->original);
        st->original = prev;
        g_clear_object(&st->preview);
    }
    gtk_widget_set_sensitive(st->undo_btn, st->undo_stack->len > 0);
    if (st->picture) {
        GdkPixbuf *src = st->preview ? st->preview : st->original;
        if (src) {
            GdkTexture *t = gdk_texture_new_for_pixbuf(src);
            gtk_picture_set_paintable(GTK_PICTURE(st->picture), GDK_PAINTABLE(t));
            g_object_unref(t);
        }
    }
}

static void update_preview(FilterState *st) {
    GdkPixbuf *src = st->preview ? st->preview : st->original;
    if (!src) return;
    GdkTexture *t = gdk_texture_new_for_pixbuf(src);
    gtk_picture_set_paintable(GTK_PICTURE(st->picture), GDK_PAINTABLE(t));
    gtk_picture_set_content_fit(GTK_PICTURE(st->picture), GTK_CONTENT_FIT_CONTAIN);
    gtk_picture_set_can_shrink(GTK_PICTURE(st->picture), TRUE);
    g_object_unref(t);
}

static void clear_undo(FilterState *st) {
    image_undo_clear(st->undo_stack);
    gtk_widget_set_sensitive(st->undo_btn, FALSE);
}

static void on_save_common(FilterState *st, const char *prefix) {
    GdkPixbuf *src = st->preview ? st->preview : st->original;
    if (!src) return;
    char *base = st->path ? g_path_get_basename(st->path) : g_strdup("image.png");
    char *name = g_strdup_printf("%s_%s", prefix, base);
    image_save_pixbuf_dialog(st->root, src, name);
    g_free(name);
    g_free(base);
}

static void on_drop_common(FilterState *st, const char *path) {
    GError *e = NULL;
    GdkPixbuf *pb = gdk_pixbuf_new_from_file(path, &e);
    if (!pb) { image_show_error(st->root, e->message); g_error_free(e); return; }

    g_clear_object(&st->original);
    g_clear_object(&st->preview);
    g_free(st->path);
    clear_undo(st);

    st->original = pb;
    st->path = g_strdup(path);

    update_preview(st);
    gtk_stack_set_visible_child_name(GTK_STACK(st->stack), "editor");
}

static void filter_state_free(FilterState *st) {
    if (!st) return;
    g_clear_object(&st->original);
    g_clear_object(&st->preview);
    if (st->undo_stack) image_undo_free(st->undo_stack);
    g_free(st->path);
    g_free(st);
}

static GtkWidget *make_slider_row(const char *label, double min, double max, double step, double initial, GtkWidget **out_scale, GtkWidget **out_value_lbl, GCallback cb, gpointer user_data) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *lbl = gtk_label_new(label);
    gtk_widget_add_css_class(lbl, "dim-label");
    gtk_widget_set_size_request(lbl, 100, -1);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);

    GtkWidget *scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, min, max, step);
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

static GtkWidget *build_editor_shell(FilterState *st, GtkWidget **out_sliders_box, const char *save_prefix, GCallback on_save_cb, GCallback on_reset_cb, const char *drop_hint, ImageDropCallback on_drop_cb) {
    GtkWidget *stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_widget_set_vexpand(stack, TRUE);
    st->stack = stack;

    GtkWidget *drop_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(drop_box, 24);
    gtk_widget_set_margin_end(drop_box, 24);
    gtk_widget_set_margin_top(drop_box, 24);
    gtk_widget_set_margin_bottom(drop_box, 24);
    gtk_widget_set_vexpand(drop_box, TRUE);
    gtk_box_append(GTK_BOX(drop_box), image_build_drop_zone(drop_hint, on_drop_cb, st->root));
    gtk_stack_add_named(GTK_STACK(stack), drop_box, "drop");

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
    gtk_box_append(GTK_BOX(bar), new_img);
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
    gtk_box_append(GTK_BOX(editor), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(editor), pic);
    gtk_stack_add_named(GTK_STACK(stack), editor, "editor");
    gtk_stack_set_visible_child_name(GTK_STACK(stack), "drop");

    return stack;
}

static inline double clamp01(double v) {
    return v < 0 ? 0 : (v > 1 ? 1 : v);
}

/* ================================================================== */
/* VIGNETTE                                                           */
/* ================================================================== */

static GdkPixbuf *apply_vignette(FilterState *st) {
    GdkPixbuf *src = st->original;
    int w = gdk_pixbuf_get_width(src);
    int h = gdk_pixbuf_get_height(src);
    int n = gdk_pixbuf_get_n_channels(src);
    int stride = gdk_pixbuf_get_rowstride(src);
    guchar *px = gdk_pixbuf_get_pixels(src);

    GdkPixbuf *out = gdk_pixbuf_copy(src);
    int ostride = gdk_pixbuf_get_rowstride(out);
    guchar *opx = gdk_pixbuf_get_pixels(out);

    double cx = w / 2.0;
    double cy = h / 2.0;
    double max_dist = sqrt(cx * cx + cy * cy);
    
    double strength = st->val1;
    double radius = st->val2;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            double dx = x - cx;
            double dy = y - cy;
            double dist = sqrt(dx * dx + dy * dy);
            
            double d = dist / max_dist;
            double factor = 1.0;
            if (d > radius) {
                double diff = (d - radius) / (1.0 - radius);
                factor = 1.0 - (diff * strength);
                if (factor < 0) factor = 0;
            }

            guchar *sp = px + y * stride + x * n;
            guchar *dp = opx + y * ostride + x * n;
            dp[0] = (guchar)(sp[0] * factor);
            dp[1] = (guchar)(sp[1] * factor);
            dp[2] = (guchar)(sp[2] * factor);
            if (n == 4) dp[3] = sp[3];
        }
    }
    return out;
}

static void vig_on_val(GtkRange *r, gpointer d) {
    (void)r; FilterState *st = get_state(d);
    g_clear_object(&st->preview);
    st->preview = apply_vignette(st);
    update_preview(st);
}

static void vig_on_strength(GtkRange *r, gpointer d) {
    FilterState *st = get_state(d);
    st->val1 = gtk_range_get_value(r);
    vig_on_val(r, d);
}

static void vig_on_radius(GtkRange *r, gpointer d) {
    FilterState *st = get_state(d);
    st->val2 = gtk_range_get_value(r);
    vig_on_val(r, d);
}

static void vig_on_apply(GtkButton *b, gpointer d) {
    (void)b; FilterState *st = get_state(d);
    if (st->preview) {
        push_snapshot(st);
        g_clear_object(&st->original);
        st->original = g_object_ref(st->preview);
        g_clear_object(&st->preview);
        update_preview(st);
    }
}

static void vig_on_save(GtkButton *b, gpointer d) { (void)b; on_save_common(get_state(d), "vignette"); }
static void vig_on_drop(const char *path, gpointer d) { on_drop_common(get_state(d), path); }
static void vig_on_reset(GtkButton *b, gpointer d) { (void)b; FilterState *st = get_state(d); g_clear_object(&st->preview); update_preview(st); }
void image_vignette_on_close(GtkWidget *v) { g_object_set_data(G_OBJECT(v), "filter-state", NULL); }
static void cmd_vig_reset(GtkWidget *v) { vig_on_reset(NULL, v); }
const HelvetiaToolCommand image_vignette_commands[] = {
    { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic", .activate = cmd_vig_reset },
    { NULL }
};

GtkWidget *image_vignette_create(void) {
    FilterState *st = g_new0(FilterState, 1);
    st->undo_stack = image_undo_stack_new();
    st->val1 = 0.5;
    st->val2 = 0.5;
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;
    image_install_edit_shortcuts(root, (ImageToolCallback)on_undo, (ImageToolCallback)vig_on_reset, root);

    GtkWidget *sliders;
    GtkWidget *stack = build_editor_shell(st, &sliders, "vignette", G_CALLBACK(vig_on_save), G_CALLBACK(vig_on_reset), "Image file", vig_on_drop);

    GtkWidget *s1, *l1, *s2, *l2;
    gtk_box_append(GTK_BOX(sliders), make_slider_row("Strength", 0, 1, 0.05, 0.5, &s1, &l1, G_CALLBACK(vig_on_strength), root));
    gtk_box_append(GTK_BOX(sliders), make_slider_row("Radius", 0, 1, 0.05, 0.5, &s2, &l2, G_CALLBACK(vig_on_radius), root));

    GtkWidget *apply_btn = gtk_button_new_with_label("Apply Effect");
    gtk_widget_add_css_class(apply_btn, "suggested-action");
    gtk_widget_set_halign(apply_btn, GTK_ALIGN_END);
    g_signal_connect(apply_btn, "clicked", G_CALLBACK(vig_on_apply), root);
    gtk_box_append(GTK_BOX(sliders), apply_btn);

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "filter-state", st, (GDestroyNotify)filter_state_free);
    return root;
}

/* ================================================================== */
/* FILM GRAIN                                                         */
/* ================================================================== */

static GdkPixbuf *apply_film_grain(FilterState *st) {
    GdkPixbuf *src = st->original;
    int w = gdk_pixbuf_get_width(src);
    int h = gdk_pixbuf_get_height(src);
    int n = gdk_pixbuf_get_n_channels(src);
    int stride = gdk_pixbuf_get_rowstride(src);
    guchar *px = gdk_pixbuf_get_pixels(src);

    GdkPixbuf *out = gdk_pixbuf_copy(src);
    int ostride = gdk_pixbuf_get_rowstride(out);
    guchar *opx = gdk_pixbuf_get_pixels(out);

    double intensity = st->val1 * 255.0;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            guchar *sp = px + y * stride + x * n;
            guchar *dp = opx + y * ostride + x * n;
            
            double noise = ((double)rand() / RAND_MAX - 0.5) * intensity;
            
            int r = sp[0] + noise;
            int g = sp[1] + noise;
            int b = sp[2] + noise;
            
            dp[0] = r < 0 ? 0 : (r > 255 ? 255 : r);
            dp[1] = g < 0 ? 0 : (g > 255 ? 255 : g);
            dp[2] = b < 0 ? 0 : (b > 255 ? 255 : b);
            if (n == 4) dp[3] = sp[3];
        }
    }
    return out;
}

static void fg_on_val(GtkRange *r, gpointer d) {
    FilterState *st = get_state(d);
    st->val1 = gtk_range_get_value(r);
    g_clear_object(&st->preview);
    st->preview = apply_film_grain(st);
    update_preview(st);
}

static void fg_on_apply(GtkButton *b, gpointer d) {
    (void)b; FilterState *st = get_state(d);
    if (st->preview) {
        push_snapshot(st);
        g_clear_object(&st->original);
        st->original = g_object_ref(st->preview);
        g_clear_object(&st->preview);
        update_preview(st);
    }
}

static void fg_on_save(GtkButton *b, gpointer d) { (void)b; on_save_common(get_state(d), "grain"); }
static void fg_on_drop(const char *path, gpointer d) { on_drop_common(get_state(d), path); }
static void fg_on_reset(GtkButton *b, gpointer d) { (void)b; FilterState *st = get_state(d); g_clear_object(&st->preview); update_preview(st); }
void image_film_grain_on_close(GtkWidget *v) { g_object_set_data(G_OBJECT(v), "filter-state", NULL); }
static void cmd_fg_reset(GtkWidget *v) { fg_on_reset(NULL, v); }
const HelvetiaToolCommand image_film_grain_commands[] = {
    { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic", .activate = cmd_fg_reset },
    { NULL }
};

GtkWidget *image_film_grain_create(void) {
    FilterState *st = g_new0(FilterState, 1);
    st->undo_stack = image_undo_stack_new();
    st->val1 = 0.2;
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;
    image_install_edit_shortcuts(root, (ImageToolCallback)on_undo, (ImageToolCallback)fg_on_reset, root);

    GtkWidget *sliders;
    GtkWidget *stack = build_editor_shell(st, &sliders, "grain", G_CALLBACK(fg_on_save), G_CALLBACK(fg_on_reset), "Image file", fg_on_drop);

    GtkWidget *scale, *lbl;
    gtk_box_append(GTK_BOX(sliders), make_slider_row("Intensity", 0, 1, 0.05, 0.2, &scale, &lbl, G_CALLBACK(fg_on_val), root));

    GtkWidget *apply_btn = gtk_button_new_with_label("Apply Effect");
    gtk_widget_add_css_class(apply_btn, "suggested-action");
    gtk_widget_set_halign(apply_btn, GTK_ALIGN_END);
    g_signal_connect(apply_btn, "clicked", G_CALLBACK(fg_on_apply), root);
    gtk_box_append(GTK_BOX(sliders), apply_btn);

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "filter-state", st, (GDestroyNotify)filter_state_free);
    return root;
}

/* ================================================================== */
/* GLOW                                                               */
/* ================================================================== */

/* Simple box blur for glow effect */
static GdkPixbuf *box_blur(GdkPixbuf *src, int radius) {
    int w = gdk_pixbuf_get_width(src);
    int h = gdk_pixbuf_get_height(src);
    int n = gdk_pixbuf_get_n_channels(src);
    int stride = gdk_pixbuf_get_rowstride(src);
    guchar *px = gdk_pixbuf_get_pixels(src);
    
    GdkPixbuf *out = gdk_pixbuf_copy(src);
    int ostride = gdk_pixbuf_get_rowstride(out);
    guchar *opx = gdk_pixbuf_get_pixels(out);
    
    GdkPixbuf *tmp = gdk_pixbuf_copy(src);
    guchar *tpx = gdk_pixbuf_get_pixels(tmp);
    
    /* Horizontal */
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int r=0, g=0, b=0, c=0;
            for (int k = -radius; k <= radius; k++) {
                int nx = x + k;
                if (nx >= 0 && nx < w) {
                    guchar *sp = px + y * stride + nx * n;
                    r += sp[0]; g += sp[1]; b += sp[2]; c++;
                }
            }
            guchar *tp = tpx + y * stride + x * n;
            tp[0] = r/c; tp[1] = g/c; tp[2] = b/c;
        }
    }
    
    /* Vertical */
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int r=0, g=0, b=0, c=0;
            for (int k = -radius; k <= radius; k++) {
                int ny = y + k;
                if (ny >= 0 && ny < h) {
                    guchar *tp = tpx + ny * stride + x * n;
                    r += tp[0]; g += tp[1]; b += tp[2]; c++;
                }
            }
            guchar *dp = opx + y * ostride + x * n;
            dp[0] = r/c; dp[1] = g/c; dp[2] = b/c;
        }
    }
    
    g_object_unref(tmp);
    return out;
}

static GdkPixbuf *apply_glow(FilterState *st) {
    GdkPixbuf *src = st->original;
    int radius = (int)(st->val2 * 30);
    if (radius < 1) return gdk_pixbuf_copy(src);
    
    GdkPixbuf *blurred = box_blur(src, radius);
    int w = gdk_pixbuf_get_width(src);
    int h = gdk_pixbuf_get_height(src);
    int n = gdk_pixbuf_get_n_channels(src);
    int stride = gdk_pixbuf_get_rowstride(src);
    guchar *px = gdk_pixbuf_get_pixels(src);
    guchar *bpx = gdk_pixbuf_get_pixels(blurred);

    GdkPixbuf *out = gdk_pixbuf_copy(src);
    int ostride = gdk_pixbuf_get_rowstride(out);
    guchar *opx = gdk_pixbuf_get_pixels(out);
    
    double strength = st->val1;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            guchar *sp = px + y * stride + x * n;
            guchar *bp = bpx + y * stride + x * n;
            guchar *dp = opx + y * ostride + x * n;
            
            /* Screen blend */
            double r1 = sp[0]/255.0, g1 = sp[1]/255.0, b1 = sp[2]/255.0;
            double r2 = bp[0]/255.0, g2 = bp[1]/255.0, b2 = bp[2]/255.0;
            
            double R = 1.0 - (1.0 - r1) * (1.0 - r2);
            double G = 1.0 - (1.0 - g1) * (1.0 - g2);
            double B = 1.0 - (1.0 - b1) * (1.0 - b2);
            
            dp[0] = (guchar)((r1 * (1.0 - strength) + R * strength) * 255.0);
            dp[1] = (guchar)((g1 * (1.0 - strength) + G * strength) * 255.0);
            dp[2] = (guchar)((b1 * (1.0 - strength) + B * strength) * 255.0);
            if (n == 4) dp[3] = sp[3];
        }
    }
    
    g_object_unref(blurred);
    return out;
}

static void glow_on_val(GtkRange *r, gpointer d) {
    (void)r; FilterState *st = get_state(d);
    g_clear_object(&st->preview);
    st->preview = apply_glow(st);
    update_preview(st);
}

static void glow_on_strength(GtkRange *r, gpointer d) {
    FilterState *st = get_state(d);
    st->val1 = gtk_range_get_value(r);
    glow_on_val(r, d);
}

static void glow_on_radius(GtkRange *r, gpointer d) {
    FilterState *st = get_state(d);
    st->val2 = gtk_range_get_value(r);
    glow_on_val(r, d);
}

static void glow_on_apply(GtkButton *b, gpointer d) {
    (void)b; FilterState *st = get_state(d);
    if (st->preview) {
        push_snapshot(st);
        g_clear_object(&st->original);
        st->original = g_object_ref(st->preview);
        g_clear_object(&st->preview);
        update_preview(st);
    }
}

static void glow_on_save(GtkButton *b, gpointer d) { (void)b; on_save_common(get_state(d), "glow"); }
static void glow_on_drop(const char *path, gpointer d) { on_drop_common(get_state(d), path); }
static void glow_on_reset(GtkButton *b, gpointer d) { (void)b; FilterState *st = get_state(d); g_clear_object(&st->preview); update_preview(st); }
void image_glow_on_close(GtkWidget *v) { g_object_set_data(G_OBJECT(v), "filter-state", NULL); }
static void cmd_glow_reset(GtkWidget *v) { glow_on_reset(NULL, v); }
const HelvetiaToolCommand image_glow_commands[] = {
    { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic", .activate = cmd_glow_reset },
    { NULL }
};

GtkWidget *image_glow_create(void) {
    FilterState *st = g_new0(FilterState, 1);
    st->undo_stack = image_undo_stack_new();
    st->val1 = 0.5;
    st->val2 = 0.5;
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;
    image_install_edit_shortcuts(root, (ImageToolCallback)on_undo, (ImageToolCallback)glow_on_reset, root);

    GtkWidget *sliders;
    GtkWidget *stack = build_editor_shell(st, &sliders, "glow", G_CALLBACK(glow_on_save), G_CALLBACK(glow_on_reset), "Image file", glow_on_drop);

    GtkWidget *s1, *l1, *s2, *l2;
    gtk_box_append(GTK_BOX(sliders), make_slider_row("Strength", 0, 1, 0.05, 0.5, &s1, &l1, G_CALLBACK(glow_on_strength), root));
    gtk_box_append(GTK_BOX(sliders), make_slider_row("Radius", 0, 1, 0.05, 0.5, &s2, &l2, G_CALLBACK(glow_on_radius), root));

    GtkWidget *apply_btn = gtk_button_new_with_label("Apply Effect");
    gtk_widget_add_css_class(apply_btn, "suggested-action");
    gtk_widget_set_halign(apply_btn, GTK_ALIGN_END);
    g_signal_connect(apply_btn, "clicked", G_CALLBACK(glow_on_apply), root);
    gtk_box_append(GTK_BOX(sliders), apply_btn);

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "filter-state", st, (GDestroyNotify)filter_state_free);
    return root;
}
