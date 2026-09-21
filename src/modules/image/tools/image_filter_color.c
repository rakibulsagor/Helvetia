#include <gtk/gtk.h>
#include <adwaita.h>
#include <math.h>
#include <string.h>
#include "../image_shared.h"
#include "image_filter_color.h"

/* ================================================================== */
/* COMMON STATE                                                       */
/* ================================================================== */

typedef struct {
    GdkPixbuf *original;
    GdkPixbuf *preview;
    GPtrArray *undo_stack;
    char      *path;

    double     value; /* Threshold / Posterize / Sepia intensity */

    GtkWidget *stack, *picture, *root;
    GtkWidget *undo_btn;
} FilterState;

static FilterState *get_state(GtkWidget *v) {
    return g_object_get_data(G_OBJECT(v), "filter-state");
}

static void push_snapshot(FilterState *st) {
    /* For filters with a single slider, we just need to push the image to undo */
    image_undo_push(st->undo_stack, st->original);
    gtk_widget_set_sensitive(st->undo_btn, TRUE);
}

static void on_undo(GtkButton *b, gpointer d) {
    (void)b;
    FilterState *st = get_state(d);
    if (!st->undo_stack || st->undo_stack->len == 0) return;
    
    GdkPixbuf *prev = image_undo_pop(st->undo_stack);
    if (prev) {
        g_clear_object(&st->original);
        st->original = prev;
        g_clear_object(&st->preview);
    }
    gtk_widget_set_sensitive(st->undo_btn, st->undo_stack->len > 0);
    
    /* Force UI update if picture exists */
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

static inline double clamp01(double v) {
    return v < 0 ? 0 : (v > 1 ? 1 : v);
}

typedef void (*PixelFunc)(FilterState *st, guchar *r, guchar *g, guchar *b);

static GdkPixbuf *apply_pixel_effect(FilterState *st, PixelFunc func) {
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

            func(st, &r, &g, &b);

            dp[0] = r; dp[1] = g; dp[2] = b;
            if (n == 4) dp[3] = sp[3];
        }
    }
    return out;
}

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

static GtkWidget *build_editor_shell(FilterState *st,
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

    GtkWidget *drop_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(drop_box, 24);
    gtk_widget_set_margin_end(drop_box, 24);
    gtk_widget_set_margin_top(drop_box, 24);
    gtk_widget_set_margin_bottom(drop_box, 24);
    gtk_widget_set_vexpand(drop_box, TRUE);
    gtk_box_append(GTK_BOX(drop_box),
        image_build_drop_zone(drop_hint, on_drop_cb, st->root));
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

    g_object_set_data(G_OBJECT(reset), "save_prefix", (gpointer)save_prefix);

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

/* ================================================================== */
/* SEPIA                                                              */
/* ================================================================== */

static void pixel_sepia(FilterState *st, guchar *r, guchar *g, guchar *b) {
    double R = *r / 255.0, G = *g / 255.0, B = *b / 255.0;
    double tR = (R * 0.393) + (G * 0.769) + (B * 0.189);
    double tG = (R * 0.349) + (G * 0.686) + (B * 0.168);
    double tB = (R * 0.272) + (G * 0.534) + (B * 0.131);
    
    double f = st->value / 100.0;
    R = R * (1.0 - f) + clamp01(tR) * f;
    G = G * (1.0 - f) + clamp01(tG) * f;
    B = B * (1.0 - f) + clamp01(tB) * f;
    
    *r = (guchar)(R * 255.0 + 0.5);
    *g = (guchar)(G * 255.0 + 0.5);
    *b = (guchar)(B * 255.0 + 0.5);
}

static void sepia_on_val(GtkRange *r, gpointer d) {
    FilterState *st = get_state(d);
    double v = gtk_range_get_value(r);
    if (fabs(v - st->value) < 0.5) return;
    st->value = v;
    g_clear_object(&st->preview);
    st->preview = apply_pixel_effect(st, pixel_sepia);
    update_preview(st);
}

static void sepia_on_apply(GtkButton *b, gpointer d) {
    (void)b;
    FilterState *st = get_state(d);
    if (st->preview) {
        push_snapshot(st);
        g_clear_object(&st->original);
        st->original = g_object_ref(st->preview);
        g_clear_object(&st->preview);
        st->value = 0;
        update_preview(st);
    }
}

static void sepia_on_save(GtkButton *b, gpointer d) {
    (void)b; on_save_common(get_state(d), "sepia");
}
static void sepia_on_drop(const char *path, gpointer d) {
    on_drop_common(get_state(d), path);
}
static void sepia_on_reset(GtkButton *b, gpointer d) {
    (void)b; FilterState *st = get_state(d);
    st->value = 0;
    g_clear_object(&st->preview);
    update_preview(st);
}

void image_sepia_on_close(GtkWidget *v) { g_object_set_data(G_OBJECT(v), "filter-state", NULL); }
static void cmd_sepia_reset(GtkWidget *v) { sepia_on_reset(NULL, v); }
const HelvetiaToolCommand image_sepia_commands[] = {
    { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic", .activate = cmd_sepia_reset },
    { NULL }
};

GtkWidget *image_sepia_create(void) {
    FilterState *st = g_new0(FilterState, 1);
    st->undo_stack = image_undo_stack_new();
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;
    image_install_edit_shortcuts(root, (ImageToolCallback)on_undo, (ImageToolCallback)sepia_on_reset, root);

    GtkWidget *sliders;
    GtkWidget *stack = build_editor_shell(st, &sliders, "sepia", G_CALLBACK(sepia_on_save), G_CALLBACK(sepia_on_reset), "Image file", sepia_on_drop);

    GtkWidget *scale, *lbl;
    gtk_box_append(GTK_BOX(sliders), make_slider_row("Intensity", 0, 100, 1, 0, &scale, &lbl, G_CALLBACK(sepia_on_val), root));

    GtkWidget *apply_btn = gtk_button_new_with_label("Apply Effect");
    gtk_widget_add_css_class(apply_btn, "suggested-action");
    gtk_widget_set_halign(apply_btn, GTK_ALIGN_END);
    g_signal_connect(apply_btn, "clicked", G_CALLBACK(sepia_on_apply), root);
    gtk_box_append(GTK_BOX(sliders), apply_btn);

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "filter-state", st, (GDestroyNotify)filter_state_free);
    return root;
}

/* ================================================================== */
/* GRAYSCALE                                                          */
/* ================================================================== */

static void pixel_grayscale(FilterState *st, guchar *r, guchar *g, guchar *b) {
    (void)st;
    double R = *r / 255.0, G = *g / 255.0, B = *b / 255.0;
    double Y = 0.2126 * R + 0.7152 * G + 0.0722 * B;
    *r = *g = *b = (guchar)(Y * 255.0 + 0.5);
}

static void gray_on_apply(GtkButton *b, gpointer d) {
    (void)b; FilterState *st = get_state(d);
    if (!st->original) return;
    push_snapshot(st);
    GdkPixbuf *res = apply_pixel_effect(st, pixel_grayscale);
    g_clear_object(&st->original);
    st->original = res;
    update_preview(st);
}

static void gray_on_save(GtkButton *b, gpointer d) { (void)b; on_save_common(get_state(d), "gray"); }
static void gray_on_drop(const char *path, gpointer d) { on_drop_common(get_state(d), path); }
static void gray_on_reset(GtkButton *b, gpointer d) { (void)b; FilterState *st = get_state(d); g_clear_object(&st->preview); update_preview(st); }
void image_grayscale_on_close(GtkWidget *v) { g_object_set_data(G_OBJECT(v), "filter-state", NULL); }
static void cmd_gray_reset(GtkWidget *v) { gray_on_reset(NULL, v); }
const HelvetiaToolCommand image_grayscale_commands[] = {
    { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic", .activate = cmd_gray_reset },
    { NULL }
};

GtkWidget *image_grayscale_create(void) {
    FilterState *st = g_new0(FilterState, 1);
    st->undo_stack = image_undo_stack_new();
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;
    image_install_edit_shortcuts(root, (ImageToolCallback)on_undo, (ImageToolCallback)gray_on_reset, root);

    GtkWidget *sliders;
    GtkWidget *stack = build_editor_shell(st, &sliders, "gray", G_CALLBACK(gray_on_save), G_CALLBACK(gray_on_reset), "Image file", gray_on_drop);

    GtkWidget *apply_btn = gtk_button_new_with_label("Convert to Grayscale");
    gtk_widget_add_css_class(apply_btn, "suggested-action");
    gtk_widget_set_halign(apply_btn, GTK_ALIGN_CENTER);
    g_signal_connect(apply_btn, "clicked", G_CALLBACK(gray_on_apply), root);
    gtk_box_append(GTK_BOX(sliders), apply_btn);

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "filter-state", st, (GDestroyNotify)filter_state_free);
    return root;
}

/* ================================================================== */
/* INVERT                                                             */
/* ================================================================== */

static void pixel_invert(FilterState *st, guchar *r, guchar *g, guchar *b) {
    (void)st;
    *r = 255 - *r; *g = 255 - *g; *b = 255 - *b;
}

static void invert_on_apply(GtkButton *b, gpointer d) {
    (void)b; FilterState *st = get_state(d);
    if (!st->original) return;
    push_snapshot(st);
    GdkPixbuf *res = apply_pixel_effect(st, pixel_invert);
    g_clear_object(&st->original);
    st->original = res;
    update_preview(st);
}

static void invert_on_save(GtkButton *b, gpointer d) { (void)b; on_save_common(get_state(d), "invert"); }
static void invert_on_drop(const char *path, gpointer d) { on_drop_common(get_state(d), path); }
static void invert_on_reset(GtkButton *b, gpointer d) { (void)b; FilterState *st = get_state(d); g_clear_object(&st->preview); update_preview(st); }
void image_invert_on_close(GtkWidget *v) { g_object_set_data(G_OBJECT(v), "filter-state", NULL); }
static void cmd_invert_reset(GtkWidget *v) { invert_on_reset(NULL, v); }
const HelvetiaToolCommand image_invert_commands[] = {
    { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic", .activate = cmd_invert_reset },
    { NULL }
};

GtkWidget *image_invert_create(void) {
    FilterState *st = g_new0(FilterState, 1);
    st->undo_stack = image_undo_stack_new();
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;
    image_install_edit_shortcuts(root, (ImageToolCallback)on_undo, (ImageToolCallback)invert_on_reset, root);

    GtkWidget *sliders;
    GtkWidget *stack = build_editor_shell(st, &sliders, "invert", G_CALLBACK(invert_on_save), G_CALLBACK(invert_on_reset), "Image file", invert_on_drop);

    GtkWidget *apply_btn = gtk_button_new_with_label("Invert Colors");
    gtk_widget_add_css_class(apply_btn, "suggested-action");
    gtk_widget_set_halign(apply_btn, GTK_ALIGN_CENTER);
    g_signal_connect(apply_btn, "clicked", G_CALLBACK(invert_on_apply), root);
    gtk_box_append(GTK_BOX(sliders), apply_btn);

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "filter-state", st, (GDestroyNotify)filter_state_free);
    return root;
}

/* ================================================================== */
/* THRESHOLD                                                          */
/* ================================================================== */

static void pixel_threshold(FilterState *st, guchar *r, guchar *g, guchar *b) {
    double R = *r / 255.0, G = *g / 255.0, B = *b / 255.0;
    double Y = 0.2126 * R + 0.7152 * G + 0.0722 * B;
    guchar v = (Y * 255.0 >= st->value) ? 255 : 0;
    *r = *g = *b = v;
}

static void thresh_on_val(GtkRange *r, gpointer d) {
    FilterState *st = get_state(d);
    st->value = gtk_range_get_value(r);
    g_clear_object(&st->preview);
    st->preview = apply_pixel_effect(st, pixel_threshold);
    update_preview(st);
}

static void thresh_on_apply(GtkButton *b, gpointer d) {
    (void)b; FilterState *st = get_state(d);
    if (st->preview) {
        push_snapshot(st);
        g_clear_object(&st->original);
        st->original = g_object_ref(st->preview);
        g_clear_object(&st->preview);
        update_preview(st);
    }
}

static void thresh_on_save(GtkButton *b, gpointer d) { (void)b; on_save_common(get_state(d), "thresh"); }
static void thresh_on_drop(const char *path, gpointer d) { on_drop_common(get_state(d), path); }
static void thresh_on_reset(GtkButton *b, gpointer d) { (void)b; FilterState *st = get_state(d); g_clear_object(&st->preview); update_preview(st); }
void image_threshold_on_close(GtkWidget *v) { g_object_set_data(G_OBJECT(v), "filter-state", NULL); }
static void cmd_thresh_reset(GtkWidget *v) { thresh_on_reset(NULL, v); }
const HelvetiaToolCommand image_threshold_commands[] = {
    { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic", .activate = cmd_thresh_reset },
    { NULL }
};

GtkWidget *image_threshold_create(void) {
    FilterState *st = g_new0(FilterState, 1);
    st->undo_stack = image_undo_stack_new();
    st->value = 128;
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;
    image_install_edit_shortcuts(root, (ImageToolCallback)on_undo, (ImageToolCallback)thresh_on_reset, root);

    GtkWidget *sliders;
    GtkWidget *stack = build_editor_shell(st, &sliders, "thresh", G_CALLBACK(thresh_on_save), G_CALLBACK(thresh_on_reset), "Image file", thresh_on_drop);

    GtkWidget *scale, *lbl;
    gtk_box_append(GTK_BOX(sliders), make_slider_row("Threshold", 0, 255, 1, 128, &scale, &lbl, G_CALLBACK(thresh_on_val), root));

    GtkWidget *apply_btn = gtk_button_new_with_label("Apply Effect");
    gtk_widget_add_css_class(apply_btn, "suggested-action");
    gtk_widget_set_halign(apply_btn, GTK_ALIGN_END);
    g_signal_connect(apply_btn, "clicked", G_CALLBACK(thresh_on_apply), root);
    gtk_box_append(GTK_BOX(sliders), apply_btn);

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "filter-state", st, (GDestroyNotify)filter_state_free);
    return root;
}

/* ================================================================== */
/* POSTERIZE                                                          */
/* ================================================================== */

static void pixel_posterize(FilterState *st, guchar *r, guchar *g, guchar *b) {
    double levels = st->value;
    if (levels < 2) levels = 2;
    double step = 255.0 / (levels - 1.0);
    
    *r = (guchar)(round(*r / step) * step);
    *g = (guchar)(round(*g / step) * step);
    *b = (guchar)(round(*b / step) * step);
}

static void post_on_val(GtkRange *r, gpointer d) {
    FilterState *st = get_state(d);
    st->value = gtk_range_get_value(r);
    g_clear_object(&st->preview);
    st->preview = apply_pixel_effect(st, pixel_posterize);
    update_preview(st);
}

static void post_on_apply(GtkButton *b, gpointer d) {
    (void)b; FilterState *st = get_state(d);
    if (st->preview) {
        push_snapshot(st);
        g_clear_object(&st->original);
        st->original = g_object_ref(st->preview);
        g_clear_object(&st->preview);
        update_preview(st);
    }
}

static void post_on_save(GtkButton *b, gpointer d) { (void)b; on_save_common(get_state(d), "posterize"); }
static void post_on_drop(const char *path, gpointer d) { on_drop_common(get_state(d), path); }
static void post_on_reset(GtkButton *b, gpointer d) { (void)b; FilterState *st = get_state(d); g_clear_object(&st->preview); update_preview(st); }
void image_posterize_on_close(GtkWidget *v) { g_object_set_data(G_OBJECT(v), "filter-state", NULL); }
static void cmd_post_reset(GtkWidget *v) { post_on_reset(NULL, v); }
const HelvetiaToolCommand image_posterize_commands[] = {
    { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic", .activate = cmd_post_reset },
    { NULL }
};

GtkWidget *image_posterize_create(void) {
    FilterState *st = g_new0(FilterState, 1);
    st->undo_stack = image_undo_stack_new();
    st->value = 8;
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;
    image_install_edit_shortcuts(root, (ImageToolCallback)on_undo, (ImageToolCallback)post_on_reset, root);

    GtkWidget *sliders;
    GtkWidget *stack = build_editor_shell(st, &sliders, "poster", G_CALLBACK(post_on_save), G_CALLBACK(post_on_reset), "Image file", post_on_drop);

    GtkWidget *scale, *lbl;
    gtk_box_append(GTK_BOX(sliders), make_slider_row("Levels", 2, 32, 1, 8, &scale, &lbl, G_CALLBACK(post_on_val), root));

    GtkWidget *apply_btn = gtk_button_new_with_label("Apply Effect");
    gtk_widget_add_css_class(apply_btn, "suggested-action");
    gtk_widget_set_halign(apply_btn, GTK_ALIGN_END);
    g_signal_connect(apply_btn, "clicked", G_CALLBACK(post_on_apply), root);
    gtk_box_append(GTK_BOX(sliders), apply_btn);

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "filter-state", st, (GDestroyNotify)filter_state_free);
    return root;
}
