#include <gtk/gtk.h>
#include <adwaita.h>
#include <math.h>
#include "../image_shared.h"
#include "image_autocrop.h"


/* ------------------------------------------------------------------ */
/* Common state                                                       */
/* ------------------------------------------------------------------ */

typedef struct {
    GdkPixbuf *original;
    GdkPixbuf *first_original;
    GPtrArray *undo_stack;
    GdkPixbuf *preview;
    char      *path;
    int        img_w, img_h;
    GtkWidget *stack, *picture, *root;
    GtkWidget *undo_btn;
} AcState;

static gpointer get_state(GtkWidget *v);
static void ac_update(AcState *st);

static void ac_update_undo_btn(AcState *st) {
    if (st->undo_btn)
        gtk_widget_set_sensitive(st->undo_btn, st->undo_stack && st->undo_stack->len > 0);
}

static void ac_push_undo(AcState *st) {
    if (st->original) image_undo_push(st->undo_stack, st->original);
    ac_update_undo_btn(st);
}

static void ac_undo(GtkButton *b, gpointer d) {
    (void)b;
    AcState *st = get_state(d);
    GdkPixbuf *prev = image_undo_pop(st->undo_stack);
    if (!prev) return;
    g_clear_object(&st->original);
    st->original = prev;
    st->img_w = gdk_pixbuf_get_width(prev);
    st->img_h = gdk_pixbuf_get_height(prev);
    g_clear_object(&st->preview);
    ac_update(st);
    ac_update_undo_btn(st);
}

static void ac_reset(GtkButton *b, gpointer d) {
    (void)b;
    AcState *st = get_state(d);
    if (!st->first_original) return;
    ac_push_undo(st);
    g_clear_object(&st->original);
    st->original = g_object_ref(st->first_original);
    st->img_w = gdk_pixbuf_get_width(st->original);
    st->img_h = gdk_pixbuf_get_height(st->original);
    g_clear_object(&st->preview);
    ac_update(st);
    ac_update_undo_btn(st);
}

static void ac_state_free(gpointer data) {
    AcState *st = data;
    if (!st) return;
    g_clear_object(&st->original);
    g_clear_object(&st->preview);
    g_clear_object(&st->first_original);
    if (st->undo_stack) image_undo_free(st->undo_stack);
    g_free(st->path);
    g_free(st);
}

static gpointer get_state(GtkWidget *v) {
    return g_object_get_data(G_OBJECT(v), "ac-state");
}

static void ac_update(AcState *st) {
    GdkPixbuf *src = st->preview ? st->preview : st->original;
    if (!src) return;
    GdkTexture *t = gdk_texture_new_for_pixbuf(src);
    gtk_picture_set_paintable(GTK_PICTURE(st->picture), GDK_PAINTABLE(t));
    gtk_picture_set_content_fit(GTK_PICTURE(st->picture),
                                 GTK_CONTENT_FIT_CONTAIN);
    gtk_picture_set_can_shrink(GTK_PICTURE(st->picture), TRUE);
    g_object_unref(t);
}

static void ac_load(AcState *st, const char *path) {
    GError *e = NULL;
    GdkPixbuf *pb = gdk_pixbuf_new_from_file(path, &e);
    if (!pb) { image_show_error(st->root, e->message); g_error_free(e); return; }
    g_clear_object(&st->original);
    g_clear_object(&st->preview);
    g_clear_object(&st->first_original);
    if (st->undo_stack) image_undo_clear(st->undo_stack);
    ac_update_undo_btn(st);
    g_free(st->path);

    st->original = pb;
    st->first_original = g_object_ref(pb);
    st->path = g_strdup(path);
    st->img_w = gdk_pixbuf_get_width(pb);
    st->img_h = gdk_pixbuf_get_height(pb);
    ac_update(st);
    gtk_stack_set_visible_child_name(GTK_STACK(st->stack), "editor");
}

static void ac_save(AcState *st, const char *prefix) {
    GdkPixbuf *src = st->preview ? st->preview : st->original;
    if (!src) return;
    char *base = st->path ? g_path_get_basename(st->path) : g_strdup("image.png");
    char *name = g_strdup_printf("%s_%s", prefix, base);
    image_save_pixbuf_dialog(st->root, src, name);
    g_free(name);
    g_free(base);
}

/* ------------------------------------------------------------------ */
/* STRAIGHTEN — rotate by an arbitrary small angle                    */
/* ------------------------------------------------------------------ */

/* Rotate a pixbuf by an arbitrary angle using cairo */
static GdkPixbuf *rotate_arbitrary(GdkPixbuf *src, double angle_deg) {
    int sw = gdk_pixbuf_get_width(src);
    int sh = gdk_pixbuf_get_height(src);
    double rad = angle_deg * G_PI / 180.0;

    /* Compute bounding box of the rotated rectangle */
    double c = fabs(cos(rad));
    double s = fabs(sin(rad));
    int ow = (int)ceil(sw * c + sh * s);
    int oh = (int)ceil(sw * s + sh * c);

    cairo_surface_t *surf = cairo_image_surface_create(
        CAIRO_FORMAT_ARGB32, ow, oh);
    cairo_t *cr = cairo_create(surf);

    cairo_translate(cr, ow / 2.0, oh / 2.0);
    cairo_rotate(cr, rad);
    cairo_translate(cr, -sw / 2.0, -sh / 2.0);

    gdk_cairo_set_source_pixbuf(cr, src, 0, 0);
    cairo_paint(cr);

    cairo_destroy(cr);
    cairo_surface_flush(surf);

    unsigned char *data = cairo_image_surface_get_data(surf);
    int stride = cairo_image_surface_get_stride(surf);
    GdkPixbuf *out = gdk_pixbuf_new_from_data(
        data, GDK_COLORSPACE_RGB, TRUE, 8, ow, oh, stride,
        NULL, NULL);
    /* Pixbuf references the surface memory; copy to detach */
    GdkPixbuf *copy = gdk_pixbuf_copy(out);
    cairo_surface_destroy(surf);
    return copy;
}

typedef struct {
    AcState base;
    GtkWidget *angle_scale, *angle_lbl;
} StraightenState;

static void straighten_apply(GtkButton *b, gpointer d) {
    (void)b;
    StraightenState *st = get_state(d);
    if (!st->base.original) return;

    double angle = gtk_range_get_value(GTK_RANGE(st->angle_scale));
    if (fabs(angle) < 0.01) {
        g_clear_object(&st->base.preview);
        ac_update(&st->base);
        return;
    }

    GdkPixbuf *new = rotate_arbitrary(st->base.original, angle);
    ac_push_undo(&st->base);
    g_clear_object(&st->base.original);
    st->base.original = new;
    st->base.img_w = gdk_pixbuf_get_width(new);
    st->base.img_h = gdk_pixbuf_get_height(new);
    g_clear_object(&st->base.preview);
    ac_update(&st->base);
}

static void straighten_save(GtkButton *b, gpointer d) {
    (void)b;
    ac_save(get_state(d), "straightened_");
}

static void on_angle_changed(GtkRange *r, gpointer d) {
    StraightenState *st = get_state(d);
    double v = gtk_range_get_value(r);
    char buf[32];
    snprintf(buf, sizeof buf, "%.1f°", v);
    gtk_label_set_text(GTK_LABEL(st->angle_lbl), buf);
}

void image_straighten_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "ac-state", NULL);
}

static void straighten_on_drop(const char *p, gpointer d) {
    StraightenState *st = get_state(d);
    ac_load(&st->base, p);
}

GtkWidget *image_straighten_create(void) {
    StraightenState *st = g_new0(StraightenState, 1);
    st->base.undo_stack = image_undo_stack_new();
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->base.root = root;
    image_install_edit_shortcuts(root,
        (ImageToolCallback)ac_undo,
        (ImageToolCallback)ac_reset,
        root);

    GtkWidget *stack = gtk_stack_new();
    gtk_widget_set_vexpand(stack, TRUE);
    st->base.stack = stack;

    GtkWidget *drop_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(drop_box, 24);
    gtk_widget_set_margin_end(drop_box, 24);
    gtk_widget_set_margin_top(drop_box, 24);
    gtk_widget_set_margin_bottom(drop_box, 24);
    gtk_widget_set_vexpand(drop_box, TRUE);
    gtk_box_append(GTK_BOX(drop_box),
        image_build_drop_zone("Image file", straighten_on_drop, root));
    gtk_stack_add_named(GTK_STACK(stack), drop_box, "drop");

    GtkWidget *editor = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(bar, 12);
    gtk_widget_set_margin_end(bar, 12);
    gtk_widget_set_margin_top(bar, 8);
    gtk_widget_set_margin_bottom(bar, 8);

    gtk_box_append(GTK_BOX(bar), gtk_label_new("Angle:"));
    st->angle_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,
                                                -45, 45, 0.5);
    gtk_widget_set_size_request(st->angle_scale, 240, -1);
    gtk_range_set_value(GTK_RANGE(st->angle_scale), 0);
    gtk_box_append(GTK_BOX(bar), st->angle_scale);

    st->angle_lbl = gtk_label_new("0.0°");
    gtk_widget_set_size_request(st->angle_lbl, 60, -1);
    gtk_box_append(GTK_BOX(bar), st->angle_lbl);

    GtkWidget *sp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(sp, TRUE);
    gtk_box_append(GTK_BOX(bar), sp);

    st->base.undo_btn = image_undo_button(G_CALLBACK(ac_undo), root);
    GtkWidget *reset_btn = image_reset_button(G_CALLBACK(ac_reset), root);
    gtk_box_append(GTK_BOX(bar), image_new_image_button(straighten_on_drop, root));
    gtk_box_append(GTK_BOX(bar), st->base.undo_btn);
    gtk_box_append(GTK_BOX(bar), reset_btn);


    GtkWidget *apply = gtk_button_new_with_label("Apply");
    gtk_widget_add_css_class(apply, "suggested-action");
    gtk_box_append(GTK_BOX(bar), apply);
    GtkWidget *save = gtk_button_new_with_label("Save As…");
    gtk_widget_add_css_class(save, "flat");
    gtk_box_append(GTK_BOX(bar), save);

    GtkWidget *pic = gtk_picture_new();
    gtk_widget_set_vexpand(pic, TRUE);
    st->base.picture = pic;

    gtk_box_append(GTK_BOX(editor), bar);
    gtk_box_append(GTK_BOX(editor),
                   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(editor), pic);
    gtk_stack_add_named(GTK_STACK(stack), editor, "editor");

    gtk_stack_set_visible_child_name(GTK_STACK(stack), "drop");
    gtk_box_append(GTK_BOX(root), stack);

    g_object_set_data_full(G_OBJECT(root), "ac-state", st, ac_state_free);
    g_signal_connect(st->angle_scale, "value-changed",
                     G_CALLBACK(on_angle_changed), root);
    g_signal_connect(apply, "clicked", G_CALLBACK(straighten_apply), root);
    g_signal_connect(save, "clicked", G_CALLBACK(straighten_save), root);

    return root;
}

/* ------------------------------------------------------------------ */
/* AUTO-CROP BORDERS — trim uniform border color                      */
/* ------------------------------------------------------------------ */

typedef struct {
    AcState base;
    GtkWidget *tol_scale, *tol_lbl;
    int        tolerance;
} AutocropState;

static gboolean pixel_differs(const guchar *p, const guchar *bg,
                               int channels, int tol) {
    for (int i = 0; i < channels; i++) {
        int d = abs((int)p[i] - (int)bg[i]);
        if (d > tol) return TRUE;
    }
    return FALSE;
}

static void autocrop_apply(GtkButton *b, gpointer d) {
    (void)b;
    AutocropState *st = get_state(d);
    GdkPixbuf *src = st->base.original;
    if (!src) return;

    int w = gdk_pixbuf_get_width(src);
    int h = gdk_pixbuf_get_height(src);
    int channels = gdk_pixbuf_get_n_channels(src);
    int stride = gdk_pixbuf_get_rowstride(src);
    guchar *pixels = gdk_pixbuf_get_pixels(src);
    int tol = st->tolerance;

    /* Sample background from top-left pixel */
    guchar bg[4] = {0};
    for (int i = 0; i < channels; i++) bg[i] = pixels[i];

    int top = 0, bottom = h - 1, left = 0, right = w - 1;

    /* Find top */
    for (int y = 0; y < h; y++) {
        gboolean all_bg = TRUE;
        for (int x = 0; x < w; x++) {
            if (pixel_differs(pixels + y * stride + x * channels, bg, channels, tol)) {
                all_bg = FALSE;
                break;
            }
        }
        if (!all_bg) { top = y; break; }
    }

    /* Find bottom */
    for (int y = h - 1; y >= top; y--) {
        gboolean all_bg = TRUE;
        for (int x = 0; x < w; x++) {
            if (pixel_differs(pixels + y * stride + x * channels, bg, channels, tol)) {
                all_bg = FALSE;
                break;
            }
        }
        if (!all_bg) { bottom = y; break; }
    }

    /* Find left */
    for (int x = 0; x < w; x++) {
        gboolean all_bg = TRUE;
        for (int y = top; y <= bottom; y++) {
            if (pixel_differs(pixels + y * stride + x * channels, bg, channels, tol)) {
                all_bg = FALSE;
                break;
            }
        }
        if (!all_bg) { left = x; break; }
    }

    /* Find right */
    for (int x = w - 1; x >= left; x--) {
        gboolean all_bg = TRUE;
        for (int y = top; y <= bottom; y++) {
            if (pixel_differs(pixels + y * stride + x * channels, bg, channels, tol)) {
                all_bg = FALSE;
                break;
            }
        }
        if (!all_bg) { right = x; break; }
    }

    int cw = right - left + 1;
    int ch = bottom - top + 1;
    if (cw < 1 || ch < 1) {
        image_show_error(st->base.root, "Nothing to crop");
        return;
    }

    GdkPixbuf *cropped = gdk_pixbuf_new(
        gdk_pixbuf_get_colorspace(src),
        gdk_pixbuf_get_has_alpha(src),
        gdk_pixbuf_get_bits_per_sample(src),
        cw, ch);
    gdk_pixbuf_copy_area(src, left, top, cw, ch, cropped, 0, 0);

    ac_push_undo(&st->base);
    g_clear_object(&st->base.original);
    st->base.original = cropped;
    st->base.img_w = cw;
    st->base.img_h = ch;
    g_clear_object(&st->base.preview);
    ac_update(&st->base);

    char *m = g_strdup_printf("Cropped to %d × %d", cw, ch);
    image_show_info(st->base.root, m);
    g_free(m);
}

static void autocrop_save(GtkButton *b, gpointer d) {
    (void)b;
    ac_save(get_state(d), "cropped_");
}

static void on_tol_changed(GtkRange *r, gpointer d) {
    AutocropState *st = get_state(d);
    st->tolerance = (int)gtk_range_get_value(r);
    char buf[16];
    snprintf(buf, sizeof buf, "%d", st->tolerance);
    gtk_label_set_text(GTK_LABEL(st->tol_lbl), buf);
}

void image_autocrop_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "ac-state", NULL);
}

static void autocrop_on_drop(const char *p, gpointer d) {
    AutocropState *st = get_state(d);
    ac_load(&st->base, p);
}

GtkWidget *image_autocrop_create(void) {
    AutocropState *st = g_new0(AutocropState, 1);
    st->base.undo_stack = image_undo_stack_new();
    st->tolerance = 20;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->base.root = root;
    image_install_edit_shortcuts(root,
        (ImageToolCallback)ac_undo,
        (ImageToolCallback)ac_reset,
        root);

    GtkWidget *stack = gtk_stack_new();
    gtk_widget_set_vexpand(stack, TRUE);
    st->base.stack = stack;

    GtkWidget *drop_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(drop_box, 24);
    gtk_widget_set_margin_end(drop_box, 24);
    gtk_widget_set_margin_top(drop_box, 24);
    gtk_widget_set_margin_bottom(drop_box, 24);
    gtk_widget_set_vexpand(drop_box, TRUE);
    gtk_box_append(GTK_BOX(drop_box),
        image_build_drop_zone("Image with uniform border", autocrop_on_drop, root));
    gtk_stack_add_named(GTK_STACK(stack), drop_box, "drop");

    GtkWidget *editor = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(bar, 12);
    gtk_widget_set_margin_end(bar, 12);
    gtk_widget_set_margin_top(bar, 8);
    gtk_widget_set_margin_bottom(bar, 8);

    gtk_box_append(GTK_BOX(bar), gtk_label_new("Tolerance:"));
    st->tol_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,
                                              0, 100, 1);
    gtk_widget_set_size_request(st->tol_scale, 200, -1);
    gtk_range_set_value(GTK_RANGE(st->tol_scale), 20);
    gtk_box_append(GTK_BOX(bar), st->tol_scale);

    st->tol_lbl = gtk_label_new("20");
    gtk_widget_set_size_request(st->tol_lbl, 40, -1);
    gtk_box_append(GTK_BOX(bar), st->tol_lbl);

    GtkWidget *sp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(sp, TRUE);
    gtk_box_append(GTK_BOX(bar), sp);

    st->base.undo_btn = image_undo_button(G_CALLBACK(ac_undo), root);
    GtkWidget *reset_btn = image_reset_button(G_CALLBACK(ac_reset), root);
    gtk_box_append(GTK_BOX(bar), image_new_image_button(autocrop_on_drop, root));
    gtk_box_append(GTK_BOX(bar), st->base.undo_btn);
    gtk_box_append(GTK_BOX(bar), reset_btn);


    GtkWidget *apply = gtk_button_new_with_label("Auto-crop");
    gtk_widget_add_css_class(apply, "suggested-action");
    gtk_box_append(GTK_BOX(bar), apply);
    GtkWidget *save = gtk_button_new_with_label("Save As…");
    gtk_widget_add_css_class(save, "flat");
    gtk_box_append(GTK_BOX(bar), save);

    GtkWidget *pic = gtk_picture_new();
    gtk_widget_set_vexpand(pic, TRUE);
    st->base.picture = pic;

    gtk_box_append(GTK_BOX(editor), bar);
    gtk_box_append(GTK_BOX(editor),
                   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(editor), pic);
    gtk_stack_add_named(GTK_STACK(stack), editor, "editor");

    gtk_stack_set_visible_child_name(GTK_STACK(stack), "drop");
    gtk_box_append(GTK_BOX(root), stack);

    g_object_set_data_full(G_OBJECT(root), "ac-state", st, ac_state_free);
    g_signal_connect(st->tol_scale, "value-changed",
                     G_CALLBACK(on_tol_changed), root);
    g_signal_connect(apply, "clicked", G_CALLBACK(autocrop_apply), root);
    g_signal_connect(save, "clicked", G_CALLBACK(autocrop_save), root);

    return root;
}
