#include <gtk/gtk.h>
#include <adwaita.h>
#include "../image_shared.h"
#include "image_perspective.h"

typedef struct {
    GdkPixbuf *original;
    GdkPixbuf *first_original;
    GPtrArray *undo_stack;
    GdkPixbuf *preview;
    char      *path;
    int        img_w, img_h;
    double     tl_x, tl_y, tr_x, tr_y, bl_x, bl_y, br_x, br_y;
    double     zoom;
    GtkWidget *stack, *picture, *root;
    GtkWidget *undo_btn;
} PerspState;

static PerspState *get_state(GtkWidget *v);
static void update_preview(PerspState *st);

static void persp_update_undo_btn(PerspState *st) {
    if (st->undo_btn)
        gtk_widget_set_sensitive(st->undo_btn, st->undo_stack && st->undo_stack->len > 0);
}

static void persp_push_undo(PerspState *st) {
    if (st->original) image_undo_push(st->undo_stack, st->original);
    persp_update_undo_btn(st);
}

static void persp_undo(GtkButton *b, gpointer d) {
    (void)b;
    PerspState *st = get_state(d);
    GdkPixbuf *prev = image_undo_pop(st->undo_stack);
    if (!prev) return;
    g_clear_object(&st->original);
    st->original = prev;
    st->img_w = gdk_pixbuf_get_width(prev);
    st->img_h = gdk_pixbuf_get_height(prev);
    g_clear_object(&st->preview);
    update_preview(st);
    persp_update_undo_btn(st);
}

static void persp_reset(GtkButton *b, gpointer d) {
    (void)b;
    PerspState *st = get_state(d);
    if (!st->first_original) return;
    persp_push_undo(st);
    g_clear_object(&st->original);
    st->original = g_object_ref(st->first_original);
    st->img_w = gdk_pixbuf_get_width(st->original);
    st->img_h = gdk_pixbuf_get_height(st->original);
    g_clear_object(&st->preview);
    update_preview(st);
    persp_update_undo_btn(st);
}

static void persp_state_free(gpointer data) {
    PerspState *st = data;
    if (!st) return;
    g_clear_object(&st->original);
    g_clear_object(&st->preview);
    g_clear_object(&st->first_original);
    if (st->undo_stack) image_undo_free(st->undo_stack);
    g_free(st->path);
    g_free(st);
}

static PerspState *get_state(GtkWidget *v) {
    return g_object_get_data(G_OBJECT(v), "persp-state");
}

static void update_preview(PerspState *st) {
    GdkPixbuf *src = st->preview ? st->preview : st->original;
    if (!src) return;
    gtk_picture_set_paintable(GTK_PICTURE(st->picture), GDK_PAINTABLE(gdk_texture_new_for_pixbuf(src)));
    gtk_picture_set_content_fit(GTK_PICTURE(st->picture),
                                 GTK_CONTENT_FIT_CONTAIN);
    gtk_picture_set_can_shrink(GTK_PICTURE(st->picture), TRUE);
    
}

static GdkPixbuf *apply_perspective(PerspState *st) {
    GdkPixbuf *src = st->original;
    int sw = gdk_pixbuf_get_width(src);
    int sh = gdk_pixbuf_get_height(src);
    int channels = gdk_pixbuf_get_n_channels(src);
    int sstride = gdk_pixbuf_get_rowstride(src);
    guchar *spx = gdk_pixbuf_get_pixels(src);

    GdkPixbuf *out = gdk_pixbuf_new(gdk_pixbuf_get_colorspace(src),
                                     gdk_pixbuf_get_has_alpha(src), 8,
                                     sw, sh);
    gdk_pixbuf_fill(out, 0x00000000);
    int ostride = gdk_pixbuf_get_rowstride(out);
    guchar *opx = gdk_pixbuf_get_pixels(out);

    for (int oy = 0; oy < sh; oy++) {
        double v = (double)oy / (sh - 1);
        for (int ox = 0; ox < sw; ox++) {
            double u = (double)ox / (sw - 1);
            double x = (1-u)*(1-v) * st->tl_x +
                       u*(1-v)     * st->tr_x +
                       (1-u)*v     * st->bl_x +
                       u*v         * st->br_x;
            double y = (1-u)*(1-v) * st->tl_y +
                       u*(1-v)     * st->tr_y +
                       (1-u)*v     * st->bl_y +
                       u*v         * st->br_y;

            int ix = (int)(x * (sw - 1));
            int iy = (int)(y * (sh - 1));
            if (ix < 0 || ix >= sw || iy < 0 || iy >= sh) continue;

            guchar *src_px = spx + iy * sstride + ix * channels;
            guchar *dst_px = opx + oy * ostride + ox * channels;
            for (int c = 0; c < channels; c++)
                dst_px[c] = src_px[c];
            if (channels == 3)
                dst_px[3] = 255;
        }
    }
    return out;
}

typedef struct { double *val; } SliderRef;

static void on_slider(GtkRange *r, gpointer d) {
    (void)d;
    SliderRef *ref = g_object_get_data(G_OBJECT(r), "ref");
    *ref->val = gtk_range_get_value(r);
}

static void apply_persp(GtkButton *b, gpointer d) {
    (void)b;
    PerspState *st = get_state(d);
    if (!st->original) return;
    
    GdkPixbuf *new = apply_perspective(st);
    persp_push_undo(st);
    g_clear_object(&st->original);
    st->original = new;
    st->img_w = gdk_pixbuf_get_width(new);
    st->img_h = gdk_pixbuf_get_height(new);
    g_clear_object(&st->preview);
    update_preview(st);
}

static void save_persp(GtkButton *b, gpointer d) {
    (void)b;
    PerspState *st = get_state(d);
    GdkPixbuf *src = st->preview ? st->preview : st->original;
    if (!src) return;
    char *base = st->path ? g_path_get_basename(st->path) : g_strdup("image.png");
    char *name = g_strdup_printf("perspective_%s", base);
    image_save_pixbuf_dialog(st->root, src, name);
    g_free(name);
    g_free(base);
}

static void on_drop(const char *path, gpointer d) {
    PerspState *st = get_state(d);
    GError *e = NULL;
    GdkPixbuf *pb = gdk_pixbuf_new_from_file(path, &e);
    if (!pb) { image_show_error(st->root, e->message); g_error_free(e); return; }

    g_clear_object(&st->original);
    g_clear_object(&st->preview);
    g_clear_object(&st->first_original);
    if (st->undo_stack) image_undo_clear(st->undo_stack);
    persp_update_undo_btn(st);
    g_free(st->path);

    st->original = pb;
    st->first_original = gdk_pixbuf_copy(pb);
    st->path = g_strdup(path);
    st->img_w = gdk_pixbuf_get_width(pb);
    st->img_h = gdk_pixbuf_get_height(pb);

    st->tl_x = 0; st->tl_y = 0;
    st->tr_x = 1; st->tr_y = 0;
    st->bl_x = 0; st->bl_y = 1;
    st->br_x = 1; st->br_y = 1;

    update_preview(st);
    gtk_stack_set_visible_child_name(GTK_STACK(st->stack), "editor");
}

void image_perspective_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "persp-state", NULL);
}

static GtkWidget *make_slider(const char *label, double *val,
                               double min, double max, gpointer root) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget *lbl = gtk_label_new(label);
    gtk_widget_set_size_request(lbl, 60, -1);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl);

    GtkWidget *scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,
                                                 min, max, 0.01);
    gtk_widget_set_size_request(scale, 120, -1);
    gtk_range_set_value(GTK_RANGE(scale), *val);
    gtk_box_append(GTK_BOX(box), scale);

    SliderRef *ref = g_new0(SliderRef, 1);
    ref->val = val;
    g_object_set_data_full(G_OBJECT(scale), "ref", ref, g_free);
    g_signal_connect(scale, "value-changed", G_CALLBACK(on_slider), root);
    return box;
}

GtkWidget *image_perspective_create(void) {
    PerspState *st = g_new0(PerspState, 1);
    st->undo_stack = image_undo_stack_new();

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;
    image_install_edit_shortcuts(root,
        (ImageToolCallback)persp_undo,
        (ImageToolCallback)persp_reset,
        root);

    GtkWidget *stack = gtk_stack_new();
    gtk_widget_set_vexpand(stack, TRUE);
    st->stack = stack;

    GtkWidget *drop_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(drop_box, 24);
    gtk_widget_set_margin_end(drop_box, 24);
    gtk_widget_set_margin_top(drop_box, 24);
    gtk_widget_set_margin_bottom(drop_box, 24);
    gtk_widget_set_vexpand(drop_box, TRUE);
    gtk_box_append(GTK_BOX(drop_box),
        image_build_drop_zone("Image file", on_drop, root));
    gtk_stack_add_named(GTK_STACK(stack), drop_box, "drop");

    GtkWidget *editor = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    GtkWidget *opts = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start(opts, 12);
    gtk_widget_set_margin_end(opts, 12);
    gtk_widget_set_margin_top(opts, 8);
    gtk_widget_set_margin_bottom(opts, 8);

    GtkWidget *top_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *top_sp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(top_sp, TRUE);
    gtk_box_append(GTK_BOX(top_row), top_sp);
    gtk_box_append(GTK_BOX(top_row),
                   image_new_image_button(on_drop, root));
    gtk_box_append(GTK_BOX(opts), top_row);

    gtk_box_append(GTK_BOX(opts), make_slider("TL X", &st->tl_x, 0, 1, root));
    gtk_box_append(GTK_BOX(opts), make_slider("TL Y", &st->tl_y, 0, 1, root));
    gtk_box_append(GTK_BOX(opts), make_slider("TR X", &st->tr_x, 0, 1, root));
    gtk_box_append(GTK_BOX(opts), make_slider("TR Y", &st->tr_y, 0, 1, root));
    gtk_box_append(GTK_BOX(opts), make_slider("BL X", &st->bl_x, 0, 1, root));
    gtk_box_append(GTK_BOX(opts), make_slider("BL Y", &st->bl_y, 0, 1, root));
    gtk_box_append(GTK_BOX(opts), make_slider("BR X", &st->br_x, 0, 1, root));
    gtk_box_append(GTK_BOX(opts), make_slider("BR Y", &st->br_y, 0, 1, root));

    GtkWidget *btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    st->undo_btn = image_undo_button(G_CALLBACK(persp_undo), root);
    GtkWidget *reset_btn = image_reset_button(G_CALLBACK(persp_reset), root);
    GtkWidget *apply = gtk_button_new_with_label("Apply");
    gtk_widget_add_css_class(apply, "suggested-action");
    GtkWidget *save = gtk_button_new_with_label("Save As…");
    gtk_widget_add_css_class(save, "flat");
    
    gtk_box_append(GTK_BOX(btn_row), st->undo_btn);
    gtk_box_append(GTK_BOX(btn_row), reset_btn);
    gtk_box_append(GTK_BOX(btn_row), apply);
    gtk_box_append(GTK_BOX(btn_row), save);
    gtk_box_append(GTK_BOX(opts), btn_row);

    GtkWidget *pic = gtk_picture_new();
    gtk_widget_set_vexpand(pic, TRUE);
    st->picture = pic;

    gtk_box_append(GTK_BOX(editor), opts);
    gtk_box_append(GTK_BOX(editor),
                   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(editor), pic);
    gtk_stack_add_named(GTK_STACK(stack), editor, "editor");

    gtk_stack_set_visible_child_name(GTK_STACK(stack), "drop");
    gtk_box_append(GTK_BOX(root), stack);

    g_object_set_data_full(G_OBJECT(root), "persp-state", st, persp_state_free);
    g_signal_connect(apply, "clicked", G_CALLBACK(apply_persp), root);
    g_signal_connect(save, "clicked", G_CALLBACK(save_persp), root);

    return root;
}
