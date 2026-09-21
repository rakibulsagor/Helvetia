#include <gtk/gtk.h>
#include <adwaita.h>
#include <math.h>

#include "../image_shared.h"
#include "image_filter_struct.h"

/* ================================================================== */
/* COMMON STATE                                                       */
/* ================================================================== */

typedef struct {
    GdkPixbuf *original;
    GdkPixbuf *preview;
    GPtrArray *undo_stack;
    char      *path;

    double     val1; /* strength or block size */

    GtkWidget *stack, *picture, *root;
    GtkWidget *undo_btn;
} FilterState;

static FilterState *get_state(GtkWidget *v) { return g_object_get_data(G_OBJECT(v), "filter-state"); }

static void push_snapshot(FilterState *st) {
    image_undo_push(st->undo_stack, st->original);
    gtk_widget_set_sensitive(st->undo_btn, TRUE);
}

static void on_undo(GtkButton *b, gpointer d) {
    (void)b; FilterState *st = get_state(d);
    if (!st->undo_stack || st->undo_stack->len == 0) return;
    GdkPixbuf *prev = image_undo_pop(st->undo_stack);
    if (prev) { g_clear_object(&st->original); st->original = prev; g_clear_object(&st->preview); }
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
    g_clear_object(&st->original); g_clear_object(&st->preview); g_free(st->path); clear_undo(st);
    st->original = pb; st->path = g_strdup(path);
    update_preview(st);
    gtk_stack_set_visible_child_name(GTK_STACK(st->stack), "editor");
}

static void filter_state_free(FilterState *st) {
    if (!st) return;
    g_clear_object(&st->original); g_clear_object(&st->preview);
    if (st->undo_stack) image_undo_free(st->undo_stack);
    g_free(st->path); g_free(st);
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
    gtk_box_append(GTK_BOX(row), lbl); gtk_box_append(GTK_BOX(row), scale); gtk_box_append(GTK_BOX(row), val);
    if (out_scale) *out_scale = scale;
    if (out_value_lbl) *out_value_lbl = val;
    if (cb) g_signal_connect(scale, "value-changed", cb, user_data);
    return row;
}

static GtkWidget *build_editor_shell(FilterState *st, GtkWidget **out_sliders_box, const char *save_prefix, GCallback on_save_cb, GCallback on_reset_cb, const char *drop_hint, ImageDropCallback on_drop_cb) {
    GtkWidget *stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_widget_set_vexpand(stack, TRUE);
    st->stack = stack;

    GtkWidget *drop_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(drop_box, 24); gtk_widget_set_margin_end(drop_box, 24);
    gtk_widget_set_margin_top(drop_box, 24); gtk_widget_set_margin_bottom(drop_box, 24);
    gtk_widget_set_vexpand(drop_box, TRUE);
    gtk_box_append(GTK_BOX(drop_box), image_build_drop_zone(drop_hint, on_drop_cb, st->root));
    gtk_stack_add_named(GTK_STACK(stack), drop_box, "drop");

    GtkWidget *editor = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    GtkWidget *sliders = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start(sliders, 12); gtk_widget_set_margin_end(sliders, 12);
    gtk_widget_set_margin_top(sliders, 8); gtk_widget_set_margin_bottom(sliders, 8);
    if (out_sliders_box) *out_sliders_box = sliders;

    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(bar, 12); gtk_widget_set_margin_end(bar, 12);
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

    gtk_box_append(GTK_BOX(bar), st->undo_btn); gtk_box_append(GTK_BOX(bar), reset);
    gtk_box_append(GTK_BOX(bar), gtk_separator_new(GTK_ORIENTATION_VERTICAL));
    gtk_box_append(GTK_BOX(bar), new_img);
    gtk_box_append(GTK_BOX(bar), sp);
    gtk_box_append(GTK_BOX(bar), save);

    g_object_set_data(G_OBJECT(reset), "save_prefix", (gpointer)save_prefix);

    GtkWidget *pic = gtk_picture_new();
    gtk_widget_set_vexpand(pic, TRUE); gtk_widget_set_halign(pic, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(pic, GTK_ALIGN_CENTER); gtk_widget_add_css_class(pic, "image-viewer-canvas");
    st->picture = pic;

    if (out_sliders_box) gtk_box_append(GTK_BOX(editor), sliders);
    gtk_box_append(GTK_BOX(editor), bar);
    gtk_box_append(GTK_BOX(editor), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(editor), pic);
    gtk_stack_add_named(GTK_STACK(stack), editor, "editor");
    gtk_stack_set_visible_child_name(GTK_STACK(stack), "drop");
    return stack;
}

static inline double clamp01(double v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static inline int clampi(int v, int min, int max) { return v < min ? min : (v > max ? max : v); }

/* ================================================================== */
/* CONVOLUTION HELPER                                                 */
/* ================================================================== */

static GdkPixbuf *apply_convolution(GdkPixbuf *src, const double matrix[3][3], double factor, double bias) {
    int w = gdk_pixbuf_get_width(src);
    int h = gdk_pixbuf_get_height(src);
    int n = gdk_pixbuf_get_n_channels(src);
    int stride = gdk_pixbuf_get_rowstride(src);
    guchar *px = gdk_pixbuf_get_pixels(src);

    GdkPixbuf *out = gdk_pixbuf_copy(src);
    int ostride = gdk_pixbuf_get_rowstride(out);
    guchar *opx = gdk_pixbuf_get_pixels(out);

    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            double r = 0, g = 0, b = 0;
            for (int ky = -1; ky <= 1; ky++) {
                for (int kx = -1; kx <= 1; kx++) {
                    guchar *sp = px + (y + ky) * stride + (x + kx) * n;
                    double m = matrix[ky + 1][kx + 1];
                    r += sp[0] * m; g += sp[1] * m; b += sp[2] * m;
                }
            }
            guchar *dp = opx + y * ostride + x * n;
            dp[0] = clampi((int)(r * factor + bias), 0, 255);
            dp[1] = clampi((int)(g * factor + bias), 0, 255);
            dp[2] = clampi((int)(b * factor + bias), 0, 255);
            if (n == 4) dp[3] = px[y * stride + x * n + 3]; /* keep original alpha */
        }
    }
    return out;
}

/* ================================================================== */
/* EMBOSS                                                             */
/* ================================================================== */

static void emb_on_apply(GtkButton *b, gpointer d) {
    (void)b; FilterState *st = get_state(d);
    if (!st->original) return;
    push_snapshot(st);
    const double mat[3][3] = {
        { -2, -1,  0 },
        { -1,  1,  1 },
        {  0,  1,  2 }
    };
    GdkPixbuf *res = apply_convolution(st->original, mat, 1.0, 0.0);
    g_clear_object(&st->original);
    st->original = res;
    update_preview(st);
}

static void emb_on_save(GtkButton *b, gpointer d) { (void)b; on_save_common(get_state(d), "emboss"); }
static void emb_on_drop(const char *path, gpointer d) { on_drop_common(get_state(d), path); }
static void emb_on_reset(GtkButton *b, gpointer d) { (void)b; FilterState *st = get_state(d); g_clear_object(&st->preview); update_preview(st); }
void image_emboss_on_close(GtkWidget *v) { g_object_set_data(G_OBJECT(v), "filter-state", NULL); }
static void cmd_emb_reset(GtkWidget *v) { emb_on_reset(NULL, v); }
const HelvetiaToolCommand image_emboss_commands[] = { { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic", .activate = cmd_emb_reset }, { NULL, NULL, NULL, NULL, NULL, NULL } };

GtkWidget *image_emboss_create(void) {
    FilterState *st = g_new0(FilterState, 1);
    st->undo_stack = image_undo_stack_new();
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0); gtk_widget_set_vexpand(root, TRUE); st->root = root;
    image_install_edit_shortcuts(root, (ImageToolCallback)on_undo, (ImageToolCallback)emb_on_reset, root);
    GtkWidget *sliders;
    GtkWidget *stack = build_editor_shell(st, &sliders, "emboss", G_CALLBACK(emb_on_save), G_CALLBACK(emb_on_reset), "Image file", emb_on_drop);
    GtkWidget *apply_btn = gtk_button_new_with_label("Apply Emboss");
    gtk_widget_add_css_class(apply_btn, "suggested-action"); gtk_widget_set_halign(apply_btn, GTK_ALIGN_CENTER);
    g_signal_connect(apply_btn, "clicked", G_CALLBACK(emb_on_apply), root);
    gtk_box_append(GTK_BOX(sliders), apply_btn);
    gtk_box_append(GTK_BOX(root), stack); g_object_set_data_full(G_OBJECT(root), "filter-state", st, (GDestroyNotify)filter_state_free);
    return root;
}

/* ================================================================== */
/* EDGE DETECT                                                        */
/* ================================================================== */

static void edge_on_apply(GtkButton *b, gpointer d) {
    (void)b; FilterState *st = get_state(d);
    if (!st->original) return;
    push_snapshot(st);
    const double mat[3][3] = {
        {  0,  1,  0 },
        {  1, -4,  1 },
        {  0,  1,  0 }
    };
    GdkPixbuf *res = apply_convolution(st->original, mat, 1.0, 128.0);
    g_clear_object(&st->original);
    st->original = res;
    update_preview(st);
}

static void edge_on_save(GtkButton *b, gpointer d) { (void)b; on_save_common(get_state(d), "edge"); }
static void edge_on_drop(const char *path, gpointer d) { on_drop_common(get_state(d), path); }
static void edge_on_reset(GtkButton *b, gpointer d) { (void)b; FilterState *st = get_state(d); g_clear_object(&st->preview); update_preview(st); }
void image_edge_detect_on_close(GtkWidget *v) { g_object_set_data(G_OBJECT(v), "filter-state", NULL); }
static void cmd_edge_reset(GtkWidget *v) { edge_on_reset(NULL, v); }
const HelvetiaToolCommand image_edge_detect_commands[] = { { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic", .activate = cmd_edge_reset }, { NULL, NULL, NULL, NULL, NULL, NULL } };

GtkWidget *image_edge_detect_create(void) {
    FilterState *st = g_new0(FilterState, 1);
    st->undo_stack = image_undo_stack_new();
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0); gtk_widget_set_vexpand(root, TRUE); st->root = root;
    image_install_edit_shortcuts(root, (ImageToolCallback)on_undo, (ImageToolCallback)edge_on_reset, root);
    GtkWidget *sliders;
    GtkWidget *stack = build_editor_shell(st, &sliders, "edge", G_CALLBACK(edge_on_save), G_CALLBACK(edge_on_reset), "Image file", edge_on_drop);
    GtkWidget *apply_btn = gtk_button_new_with_label("Detect Edges");
    gtk_widget_add_css_class(apply_btn, "suggested-action"); gtk_widget_set_halign(apply_btn, GTK_ALIGN_CENTER);
    g_signal_connect(apply_btn, "clicked", G_CALLBACK(edge_on_apply), root);
    gtk_box_append(GTK_BOX(sliders), apply_btn);
    gtk_box_append(GTK_BOX(root), stack); g_object_set_data_full(G_OBJECT(root), "filter-state", st, (GDestroyNotify)filter_state_free);
    return root;
}

/* ================================================================== */
/* PIXELATE                                                           */
/* ================================================================== */

static GdkPixbuf *apply_pixelate(FilterState *st, gboolean mosaic_mode) {
    GdkPixbuf *src = st->original;
    int w = gdk_pixbuf_get_width(src);
    int h = gdk_pixbuf_get_height(src);
    int n = gdk_pixbuf_get_n_channels(src);
    int stride = gdk_pixbuf_get_rowstride(src);
    guchar *px = gdk_pixbuf_get_pixels(src);

    GdkPixbuf *out = gdk_pixbuf_copy(src);
    int ostride = gdk_pixbuf_get_rowstride(out);
    guchar *opx = gdk_pixbuf_get_pixels(out);

    int block = (int)st->val1;
    if (block < 2) return out;

    for (int y = 0; y < h; y += block) {
        for (int x = 0; x < w; x += block) {
            int end_y = y + block > h ? h : y + block;
            int end_x = x + block > w ? w : x + block;
            
            int r=0, g=0, b=0, c=0;
            for (int by = y; by < end_y; by++) {
                for (int bx = x; bx < end_x; bx++) {
                    guchar *sp = px + by * stride + bx * n;
                    r += sp[0]; g += sp[1]; b += sp[2]; c++;
                }
            }
            if (c > 0) { r /= c; g /= c; b /= c; }

            for (int by = y; by < end_y; by++) {
                for (int bx = x; bx < end_x; bx++) {
                    guchar *dp = opx + by * ostride + bx * n;
                    
                    if (mosaic_mode && (bx == x || by == y || bx == end_x - 1 || by == end_y - 1)) {
                        /* Border */
                        dp[0] = r * 0.8; dp[1] = g * 0.8; dp[2] = b * 0.8;
                    } else {
                        dp[0] = r; dp[1] = g; dp[2] = b;
                    }
                    if (n == 4) dp[3] = px[by * stride + bx * n + 3];
                }
            }
        }
    }
    return out;
}

static void pix_on_val(GtkRange *r, gpointer d) {
    FilterState *st = get_state(d);
    st->val1 = gtk_range_get_value(r);
    g_clear_object(&st->preview);
    st->preview = apply_pixelate(st, FALSE);
    update_preview(st);
}

static void pix_on_apply(GtkButton *b, gpointer d) {
    (void)b; FilterState *st = get_state(d);
    if (st->preview) {
        push_snapshot(st);
        g_clear_object(&st->original);
        st->original = g_object_ref(st->preview);
        g_clear_object(&st->preview);
        update_preview(st);
    }
}

static void pix_on_save(GtkButton *b, gpointer d) { (void)b; on_save_common(get_state(d), "pixelate"); }
static void pix_on_drop(const char *path, gpointer d) { on_drop_common(get_state(d), path); }
static void pix_on_reset(GtkButton *b, gpointer d) { (void)b; FilterState *st = get_state(d); g_clear_object(&st->preview); update_preview(st); }
void image_pixelate_on_close(GtkWidget *v) { g_object_set_data(G_OBJECT(v), "filter-state", NULL); }
static void cmd_pix_reset(GtkWidget *v) { pix_on_reset(NULL, v); }
const HelvetiaToolCommand image_pixelate_commands[] = { { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic", .activate = cmd_pix_reset }, { NULL, NULL, NULL, NULL, NULL, NULL } };

GtkWidget *image_pixelate_create(void) {
    FilterState *st = g_new0(FilterState, 1);
    st->undo_stack = image_undo_stack_new(); st->val1 = 10;
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0); gtk_widget_set_vexpand(root, TRUE); st->root = root;
    image_install_edit_shortcuts(root, (ImageToolCallback)on_undo, (ImageToolCallback)pix_on_reset, root);
    GtkWidget *sliders;
    GtkWidget *stack = build_editor_shell(st, &sliders, "pixelate", G_CALLBACK(pix_on_save), G_CALLBACK(pix_on_reset), "Image file", pix_on_drop);
    gtk_box_append(GTK_BOX(sliders), make_slider_row("Block Size", 2, 100, 1, 10, NULL, NULL, G_CALLBACK(pix_on_val), root));
    GtkWidget *apply_btn = gtk_button_new_with_label("Apply Effect");
    gtk_widget_add_css_class(apply_btn, "suggested-action"); gtk_widget_set_halign(apply_btn, GTK_ALIGN_END);
    g_signal_connect(apply_btn, "clicked", G_CALLBACK(pix_on_apply), root);
    gtk_box_append(GTK_BOX(sliders), apply_btn);
    gtk_box_append(GTK_BOX(root), stack); g_object_set_data_full(G_OBJECT(root), "filter-state", st, (GDestroyNotify)filter_state_free);
    return root;
}

/* ================================================================== */
/* MOSAIC                                                             */
/* ================================================================== */

static void mos_on_val(GtkRange *r, gpointer d) {
    FilterState *st = get_state(d);
    st->val1 = gtk_range_get_value(r);
    g_clear_object(&st->preview);
    st->preview = apply_pixelate(st, TRUE);
    update_preview(st);
}

static void mos_on_apply(GtkButton *b, gpointer d) {
    (void)b; FilterState *st = get_state(d);
    if (st->preview) {
        push_snapshot(st);
        g_clear_object(&st->original);
        st->original = g_object_ref(st->preview);
        g_clear_object(&st->preview);
        update_preview(st);
    }
}

static void mos_on_save(GtkButton *b, gpointer d) { (void)b; on_save_common(get_state(d), "mosaic"); }
static void mos_on_drop(const char *path, gpointer d) { on_drop_common(get_state(d), path); }
static void mos_on_reset(GtkButton *b, gpointer d) { (void)b; FilterState *st = get_state(d); g_clear_object(&st->preview); update_preview(st); }
void image_mosaic_on_close(GtkWidget *v) { g_object_set_data(G_OBJECT(v), "filter-state", NULL); }
static void cmd_mos_reset(GtkWidget *v) { mos_on_reset(NULL, v); }
const HelvetiaToolCommand image_mosaic_commands[] = { { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic", .activate = cmd_mos_reset }, { NULL, NULL, NULL, NULL, NULL, NULL } };

GtkWidget *image_mosaic_create(void) {
    FilterState *st = g_new0(FilterState, 1);
    st->undo_stack = image_undo_stack_new(); st->val1 = 15;
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0); gtk_widget_set_vexpand(root, TRUE); st->root = root;
    image_install_edit_shortcuts(root, (ImageToolCallback)on_undo, (ImageToolCallback)mos_on_reset, root);
    GtkWidget *sliders;
    GtkWidget *stack = build_editor_shell(st, &sliders, "mosaic", G_CALLBACK(mos_on_save), G_CALLBACK(mos_on_reset), "Image file", mos_on_drop);
    gtk_box_append(GTK_BOX(sliders), make_slider_row("Tile Size", 2, 100, 1, 15, NULL, NULL, G_CALLBACK(mos_on_val), root));
    GtkWidget *apply_btn = gtk_button_new_with_label("Apply Effect");
    gtk_widget_add_css_class(apply_btn, "suggested-action"); gtk_widget_set_halign(apply_btn, GTK_ALIGN_END);
    g_signal_connect(apply_btn, "clicked", G_CALLBACK(mos_on_apply), root);
    gtk_box_append(GTK_BOX(sliders), apply_btn);
    gtk_box_append(GTK_BOX(root), stack); g_object_set_data_full(G_OBJECT(root), "filter-state", st, (GDestroyNotify)filter_state_free);
    return root;
}
