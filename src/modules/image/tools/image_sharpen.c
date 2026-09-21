#include <gtk/gtk.h>
#include <adwaita.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "../image_shared.h"
#include "image_sharpen.h"

/* ================================================================== */
/* SHARED STATE                                                       */
/* ================================================================== */

typedef struct {
    GdkPixbuf *original;
    GdkPixbuf *preview;
    GdkPixbuf *first_original;
    GPtrArray *undo_stack;   /* double[3] */
    char      *path;

    /* Blur */
    int        blur_type;    /* 0=gaussian, 1=box, 2=motion */
    double     radius;       /* 0..50 px */
    double     angle;        /* 0..180° for motion blur */

    /* Sharpen */
    double     sharpen_amount;   /* 0..200 % */

    /* Unsharp Mask */
    double     usm_amount;       /* 0..300 % */
    double     usm_radius;       /* 0.5..20 px */
    double     usm_threshold;    /* 0..50 */

    GtkWidget *stack, *picture, *root;
    GtkWidget *undo_btn;
} FilterState;

static FilterState *get_state(GtkWidget *v) {
    return g_object_get_data(G_OBJECT(v), "filter-state");
}

/* ================================================================== */
/* UNDO                                                               */
/* ================================================================== */

static void push_snapshot(FilterState *st) {
    double *s = g_new0(double, 3);
    s[0] = st->radius;
    s[1] = st->sharpen_amount;
    s[2] = st->usm_amount;
    g_ptr_array_add(st->undo_stack, s);
    gtk_widget_set_sensitive(st->undo_btn, TRUE);
}

static void on_undo(GtkButton *b, gpointer d) {
    (void)b;
    FilterState *st = get_state(d);
    if (st->undo_stack->len == 0) return;
    double *s = g_ptr_array_index(st->undo_stack, st->undo_stack->len - 1);
    g_ptr_array_remove_index(st->undo_stack, st->undo_stack->len - 1);
    st->radius = s[0];
    st->sharpen_amount = s[1];
    st->usm_amount = s[2];
    g_free(s);
    gtk_widget_set_sensitive(st->undo_btn, st->undo_stack->len > 0);
    /* Caller re-applies effect */
}

static void clear_undo(FilterState *st) {
    for (guint i = 0; i < st->undo_stack->len; i++)
        g_free(g_ptr_array_index(st->undo_stack, i));
    g_ptr_array_set_size(st->undo_stack, 0);
    gtk_widget_set_sensitive(st->undo_btn, FALSE);
}

/* ================================================================== */
/* SHARED HELPERS                                                     */
/* ================================================================== */

static void update_preview(FilterState *st) {
    GdkPixbuf *src = st->preview ? st->preview : st->original;
    if (!src) return;
    GdkTexture *t = gdk_texture_new_for_pixbuf(src);
    gtk_picture_set_paintable(GTK_PICTURE(st->picture), GDK_PAINTABLE(t));
    gtk_picture_set_content_fit(GTK_PICTURE(st->picture),
                                 GTK_CONTENT_FIT_CONTAIN);
    gtk_picture_set_can_shrink(GTK_PICTURE(st->picture), TRUE);
    g_object_unref(t);
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
    g_clear_object(&st->first_original);
    g_free(st->path);
    clear_undo(st);

    st->original = pb;
    st->first_original = g_object_ref(pb);
    st->path = g_strdup(path);

    st->blur_type = 0;
    st->radius = 5.0;
    st->angle = 0.0;
    st->sharpen_amount = 0;
    st->usm_amount = 0;
    st->usm_radius = 2.0;
    st->usm_threshold = 0;

    update_preview(st);
    gtk_stack_set_visible_child_name(GTK_STACK(st->stack), "editor");
}

static void filter_state_free(FilterState *st) {
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
static inline double clamp01d(double v) { return clampd(v, 0.0, 1.0); }

/* ================================================================== */
/* BLUR — box blur, gaussian approximation, motion                    */
/* ================================================================== */

/* Box blur (separable, two passes = fast gaussian approximation) */
static void box_blur_pass(const double *src, double *dst,
                           int w, int h, int radius, gboolean horizontal) {
    int n = w * h;
    int r = radius;
    if (r < 1) { memcpy(dst, src, n * sizeof(double)); return; }

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            double sum = 0;
            int count = 0;
            for (int k = -r; k <= r; k++) {
                int nx = horizontal ? x + k : x;
                int ny = horizontal ? y : y + k;
                if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                sum += src[ny * w + nx];
                count++;
            }
            dst[y * w + x] = count > 0 ? sum / count : 0;
        }
    }
}

/* Motion blur — average along a line at a given angle */
static void motion_blur(const double *src, double *dst,
                         int w, int h, int radius, double angle_deg) {
    if (radius < 1) { memcpy(dst, src, w * h * sizeof(double)); return; }
    double rad = angle_deg * G_PI / 180.0;
    double dx = cos(rad), dy = sin(rad);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            double sum = 0;
            int count = 0;
            for (int k = -radius; k <= radius; k++) {
                int nx = (int)(x + dx * k + 0.5);
                int ny = (int)(y + dy * k + 0.5);
                if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                sum += src[ny * w + nx];
                count++;
            }
            dst[y * w + x] = count > 0 ? sum / count : 0;
        }
    }
}

static GdkPixbuf *apply_blur(FilterState *st) {
    GdkPixbuf *src = st->original;
    int w = gdk_pixbuf_get_width(src);
    int h = gdk_pixbuf_get_height(src);
    int n = gdk_pixbuf_get_n_channels(src);
    int stride = gdk_pixbuf_get_rowstride(src);
    guchar *px = gdk_pixbuf_get_pixels(src);

    int radius = (int)(st->radius + 0.5);
    if (radius < 1) return g_object_ref(src);

    GdkPixbuf *out = gdk_pixbuf_copy(src);
    int ostride = gdk_pixbuf_get_rowstride(out);
    guchar *opx = gdk_pixbuf_get_pixels(out);

    int sz = w * h;
    double *chan = g_new(double, sz);
    double *tmp  = g_new(double, sz);

    for (int c = 0; c < 3; c++) {
        /* Load channel */
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                chan[y * w + x] = px[y * stride + x * n + c];

        if (st->blur_type == 2) {
            motion_blur(chan, tmp, w, h, radius, st->angle);
            memcpy(chan, tmp, sz * sizeof(double));
        } else if (st->blur_type == 1) {
            /* Single box blur */
            box_blur_pass(chan, tmp, w, h, radius, TRUE);
            box_blur_pass(tmp, chan, w, h, radius, FALSE);
        } else {
            /* Gaussian approx: 2 box passes */
            box_blur_pass(chan, tmp, w, h, radius, TRUE);
            box_blur_pass(tmp, chan, w, h, radius, FALSE);
            box_blur_pass(chan, tmp, w, h, radius, TRUE);
            box_blur_pass(tmp, chan, w, h, radius, FALSE);
        }

        /* Store result */
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                opx[y * ostride + x * n + c] =
                    (guchar)(clamp01d(chan[y * w + x] / 255.0) * 255.0 + 0.5);
    }

    g_free(chan);
    g_free(tmp);
    return out;
}

/* ================================================================== */
/* SHARPEN — 3x3 convolution kernel                                   */
/* ================================================================== */

static GdkPixbuf *apply_sharpen(FilterState *st) {
    GdkPixbuf *src = st->original;
    int w = gdk_pixbuf_get_width(src);
    int h = gdk_pixbuf_get_height(src);
    int n = gdk_pixbuf_get_n_channels(src);
    int stride = gdk_pixbuf_get_rowstride(src);
    guchar *px = gdk_pixbuf_get_pixels(src);

    double amount = st->sharpen_amount / 100.0;
    if (amount < 0.01) return g_object_ref(src);

    GdkPixbuf *out = gdk_pixbuf_copy(src);
    int ostride = gdk_pixbuf_get_rowstride(out);
    guchar *opx = gdk_pixbuf_get_pixels(out);

    /* Kernel: center = 1 + 4*amount, neighbors = -amount */
    double center = 1.0 + 4.0 * amount;

    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            for (int c = 0; c < 3; c++) {
                double v = center * px[y * stride + x * n + c]
                         - amount * (
                            px[(y-1) * stride + x * n + c] +
                            px[(y+1) * stride + x * n + c] +
                            px[y * stride + (x-1) * n + c] +
                            px[y * stride + (x+1) * n + c]);
                opx[y * ostride + x * n + c] =
                    (guchar)clampd(v + 0.5, 0, 255);
            }
        }
    }
    return out;
}

/* ================================================================== */
/* UNSHARP MASK — original + amount * (original - blurred)            */
/* ================================================================== */

static GdkPixbuf *apply_unsharp(FilterState *st) {
    GdkPixbuf *src = st->original;
    int w = gdk_pixbuf_get_width(src);
    int h = gdk_pixbuf_get_height(src);
    int n = gdk_pixbuf_get_n_channels(src);
    int stride = gdk_pixbuf_get_rowstride(src);
    guchar *px = gdk_pixbuf_get_pixels(src);

    double amount = st->usm_amount / 100.0;
    if (amount < 0.01) return g_object_ref(src);

    int radius = (int)(st->usm_radius + 0.5);
    if (radius < 1) radius = 1;
    if (radius > 40) radius = 40;

    double threshold = st->usm_threshold;

    int sz = w * h;
    double *chan = g_new(double, sz);
    double *blur = g_new(double, sz);
    double *tmp  = g_new(double, sz);

    GdkPixbuf *out = gdk_pixbuf_copy(src);
    int ostride = gdk_pixbuf_get_rowstride(out);
    guchar *opx = gdk_pixbuf_get_pixels(out);

    for (int c = 0; c < 3; c++) {
        /* Load original channel */
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                chan[y * w + x] = px[y * stride + x * n + c];

        /* Gaussian blur of the channel */
        box_blur_pass(chan, tmp, w, h, radius, TRUE);
        box_blur_pass(tmp, blur, w, h, radius, FALSE);
        box_blur_pass(blur, tmp, w, h, radius, TRUE);
        box_blur_pass(tmp, blur, w, h, radius, FALSE);

        /* Sharpen = orig + amount * (orig - blur) */
        for (int i = 0; i < sz; i++) {
            double diff = chan[i] - blur[i];
            if (fabs(diff) < threshold) diff = 0;
            double v = chan[i] + amount * diff;
            opx[(i / w) * ostride + (i % w) * n + c] =
                (guchar)clampd(v + 0.5, 0, 255);
        }
    }

    g_free(chan);
    g_free(blur);
    g_free(tmp);
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
    gtk_widget_set_size_request(lbl, 120, -1);
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

static GtkWidget *build_shell(FilterState *st, GtkWidget **out_sliders,
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

    GtkWidget *sp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(sp, TRUE);

    GtkWidget *save = gtk_button_new_with_label("Save As…");
    gtk_widget_add_css_class(save, "flat");
    if (on_save) g_signal_connect(save, "clicked", on_save, root);

    gtk_box_append(GTK_BOX(bar), st->undo_btn);
    gtk_box_append(GTK_BOX(bar), reset);
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
/* TOOL 1 — BLUR                                                      */
/* ================================================================== */

static void bl_refresh(FilterState *st) {
    g_clear_object(&st->preview);
    st->preview = apply_blur(st);
    update_preview(st);
}

static void bl_on_radius(GtkRange *r, gpointer d) {
    FilterState *st = get_state(d);
    double v = gtk_range_get_value(r);
    if (fabs(v - st->radius) < 0.5) return;
    push_snapshot(st);
    st->radius = v;
    bl_refresh(st);
}

static void bl_on_angle(GtkRange *r, gpointer d) {
    FilterState *st = get_state(d);
    st->angle = gtk_range_get_value(r);
    if (st->blur_type == 2) bl_refresh(st);
}

static void bl_on_type(GObject *dd, GParamSpec *p, gpointer d) {
    (void)p;
    FilterState *st = get_state(d);
    st->blur_type = gtk_drop_down_get_selected(GTK_DROP_DOWN(dd));
    bl_refresh(st);
}

static void bl_on_save(GtkButton *b, gpointer d) {
    (void)b; on_save_common(get_state(d), "blur");
}
static void bl_on_drop(const char *path, gpointer d) {
    FilterState *st = get_state(d);
    on_drop_common(st, path);
    if (st->original) bl_refresh(st);
}
static void bl_on_reset(GtkButton *b, gpointer d) {
    (void)b;
    FilterState *st = get_state(d);
    push_snapshot(st);
    st->radius = 5.0;
    st->angle = 0.0;
    bl_refresh(st);
}

void image_blur_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "filter-state", NULL);
}
static void cmd_bl_reset(GtkWidget *v) { bl_on_reset(NULL, v); }

const HelvetiaToolCommand image_blur_commands[] = {
    { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic",
      .accel = NULL, .tooltip = "Reset", .activate = cmd_bl_reset },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

GtkWidget *image_blur_create(void) {
    FilterState *st = g_new0(FilterState, 1);
    st->undo_stack = g_ptr_array_new();
    st->radius = 5.0;
    st->blur_type = 0;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;

    GtkWidget *sliders;
    GtkWidget *stack = build_shell(st, &sliders, "Image file",
                                    bl_on_drop, G_CALLBACK(bl_on_save),
                                    G_CALLBACK(bl_on_reset), root);

    /* Type dropdown */
    GtkWidget *type_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *type_lbl = gtk_label_new("Type:");
    gtk_widget_add_css_class(type_lbl, "dim-label");
    gtk_widget_set_size_request(type_lbl, 120, -1);
    gtk_label_set_xalign(GTK_LABEL(type_lbl), 0.0f);
    const char *types[] = {"Gaussian", "Box", "Motion", NULL};
    GtkWidget *type_dd = gtk_drop_down_new_from_strings(types);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(type_dd), 0);
    gtk_box_append(GTK_BOX(type_row), type_lbl);
    gtk_box_append(GTK_BOX(type_row), type_dd);
    gtk_box_append(GTK_BOX(sliders), type_row);

    GtkWidget *s1, *l1, *s2, *l2;
    gtk_box_append(GTK_BOX(sliders),
        make_slider("Radius", 1, 50, 1, 5, &s1, &l1,
                     G_CALLBACK(bl_on_radius), root));
    gtk_box_append(GTK_BOX(sliders),
        make_slider("Angle (motion)", 0, 180, 1, 0, &s2, &l2,
                     G_CALLBACK(bl_on_angle), root));
    gtk_label_set_text(GTK_LABEL(l1), "5");
    gtk_label_set_text(GTK_LABEL(l2), "0°");

    g_signal_connect(type_dd, "notify::selected",
                     G_CALLBACK(bl_on_type), root);

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "filter-state", st,
                           (GDestroyNotify)filter_state_free);
    return root;
}

/* ================================================================== */
/* TOOL 2 — SHARPEN                                                   */
/* ================================================================== */

static void sh_refresh(FilterState *st) {
    g_clear_object(&st->preview);
    st->preview = apply_sharpen(st);
    update_preview(st);
}

static void sh_on_amount(GtkRange *r, gpointer d) {
    FilterState *st = get_state(d);
    double v = gtk_range_get_value(r);
    if (fabs(v - st->sharpen_amount) < 0.5) return;
    push_snapshot(st);
    st->sharpen_amount = v;
    sh_refresh(st);
}

static void sh_on_save(GtkButton *b, gpointer d) {
    (void)b; on_save_common(get_state(d), "sharpen");
}
static void sh_on_drop(const char *path, gpointer d) {
    FilterState *st = get_state(d);
    on_drop_common(st, path);
    if (st->original) sh_refresh(st);
}
static void sh_on_reset(GtkButton *b, gpointer d) {
    (void)b;
    FilterState *st = get_state(d);
    push_snapshot(st);
    st->sharpen_amount = 0;
    g_clear_object(&st->preview);
    update_preview(st);
}

void image_sharpen_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "filter-state", NULL);
}
static void cmd_sh_reset(GtkWidget *v) { sh_on_reset(NULL, v); }

const HelvetiaToolCommand image_sharpen_commands[] = {
    { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic",
      .accel = NULL, .tooltip = "Reset", .activate = cmd_sh_reset },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

GtkWidget *image_sharpen_create(void) {
    FilterState *st = g_new0(FilterState, 1);
    st->undo_stack = g_ptr_array_new();
    st->sharpen_amount = 0;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;

    GtkWidget *sliders;
    GtkWidget *stack = build_shell(st, &sliders, "Image file",
                                    sh_on_drop, G_CALLBACK(sh_on_save),
                                    G_CALLBACK(sh_on_reset), root);

    GtkWidget *s, *l;
    gtk_box_append(GTK_BOX(sliders),
        make_slider("Amount %", 0, 200, 1, 0, &s, &l,
                     G_CALLBACK(sh_on_amount), root));
    gtk_label_set_text(GTK_LABEL(l), "0");

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "filter-state", st,
                           (GDestroyNotify)filter_state_free);
    return root;
}

/* ================================================================== */
/* TOOL 3 — UNSHARP MASK                                              */
/* ================================================================== */

static void us_refresh(FilterState *st) {
    g_clear_object(&st->preview);
    st->preview = apply_unsharp(st);
    update_preview(st);
}

static void us_on_amount(GtkRange *r, gpointer d) {
    FilterState *st = get_state(d);
    double v = gtk_range_get_value(r);
    if (fabs(v - st->usm_amount) < 0.5) return;
    push_snapshot(st);
    st->usm_amount = v;
    us_refresh(st);
}
static void us_on_radius(GtkRange *r, gpointer d) {
    FilterState *st = get_state(d);
    st->usm_radius = gtk_range_get_value(r);
    us_refresh(st);
}
static void us_on_threshold(GtkRange *r, gpointer d) {
    FilterState *st = get_state(d);
    st->usm_threshold = gtk_range_get_value(r);
    us_refresh(st);
}

static void us_on_save(GtkButton *b, gpointer d) {
    (void)b; on_save_common(get_state(d), "unsharp");
}
static void us_on_drop(const char *path, gpointer d) {
    FilterState *st = get_state(d);
    on_drop_common(st, path);
    if (st->original) us_refresh(st);
}
static void us_on_reset(GtkButton *b, gpointer d) {
    (void)b;
    FilterState *st = get_state(d);
    push_snapshot(st);
    st->usm_amount = 0;
    st->usm_radius = 2.0;
    st->usm_threshold = 0;
    g_clear_object(&st->preview);
    update_preview(st);
}

void image_unsharp_mask_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "filter-state", NULL);
}
static void cmd_us_reset(GtkWidget *v) { us_on_reset(NULL, v); }

const HelvetiaToolCommand image_unsharp_mask_commands[] = {
    { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic",
      .accel = NULL, .tooltip = "Reset", .activate = cmd_us_reset },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

GtkWidget *image_unsharp_mask_create(void) {
    FilterState *st = g_new0(FilterState, 1);
    st->undo_stack = g_ptr_array_new();
    st->usm_amount = 0;
    st->usm_radius = 2.0;
    st->usm_threshold = 0;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;

    GtkWidget *sliders;
    GtkWidget *stack = build_shell(st, &sliders, "Image file",
                                    us_on_drop, G_CALLBACK(us_on_save),
                                    G_CALLBACK(us_on_reset), root);

    GtkWidget *s1, *l1, *s2, *l2, *s3, *l3;
    gtk_box_append(GTK_BOX(sliders),
        make_slider("Amount %", 0, 300, 1, 0, &s1, &l1,
                     G_CALLBACK(us_on_amount), root));
    gtk_box_append(GTK_BOX(sliders),
        make_slider("Radius", 0.5, 20, 0.5, 2.0, &s2, &l2,
                     G_CALLBACK(us_on_radius), root));
    gtk_box_append(GTK_BOX(sliders),
        make_slider("Threshold", 0, 50, 1, 0, &s3, &l3,
                     G_CALLBACK(us_on_threshold), root));
    gtk_label_set_text(GTK_LABEL(l1), "0");
    gtk_label_set_text(GTK_LABEL(l2), "2.0");
    gtk_label_set_text(GTK_LABEL(l3), "0");

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "filter-state", st,
                           (GDestroyNotify)filter_state_free);
    return root;
}
