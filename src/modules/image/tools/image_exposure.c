#include <gtk/gtk.h>
#include <adwaita.h>
#include <math.h>
#include "../image_shared.h"
#include "image_exposure.h"

/* ------------------------------------------------------------------ */
/* State                                                              */
/* ------------------------------------------------------------------ */

typedef struct {
    GdkPixbuf *original;
    GdkPixbuf *preview;
    GdkPixbuf *first_original;
    GPtrArray *undo_stack;    /* {ev, black} snapshots */
    char      *path;

    double     ev;            /* -3.0 .. +3.0, in stops */
    int        black;         /* 0 .. 100, percentage of black point lift */

    GtkWidget *stack, *picture, *root;
    GtkWidget *ev_scale, *ev_lbl;
    GtkWidget *black_scale, *black_lbl;
    GtkWidget *undo_btn;
} ExpoState;

typedef struct { double ev; int black; } ExpoSnapshot;

static ExpoState *get_state(GtkWidget *v) {
    return g_object_get_data(G_OBJECT(v), "expo-state");
}

/* ------------------------------------------------------------------ */
/* Preview                                                            */
/* ------------------------------------------------------------------ */

static void update_preview(ExpoState *st) {
    GdkPixbuf *src = st->preview ? st->preview : st->original;
    if (!src) return;
    GdkTexture *t = gdk_texture_new_for_pixbuf(src);
    gtk_picture_set_paintable(GTK_PICTURE(st->picture), GDK_PAINTABLE(t));
    gtk_picture_set_content_fit(GTK_PICTURE(st->picture),
                                 GTK_CONTENT_FIT_CONTAIN);
    gtk_picture_set_can_shrink(GTK_PICTURE(st->picture), TRUE);
    g_object_unref(t);
}

static GdkPixbuf *apply_exposure(ExpoState *st) {
    GdkPixbuf *src = st->original;
    int w = gdk_pixbuf_get_width(src);
    int h = gdk_pixbuf_get_height(src);
    int n = gdk_pixbuf_get_n_channels(src);
    int stride = gdk_pixbuf_get_rowstride(src);
    guchar *px = gdk_pixbuf_get_pixels(src);

    GdkPixbuf *out = gdk_pixbuf_copy(src);
    int ostride = gdk_pixbuf_get_rowstride(out);
    guchar *opx = gdk_pixbuf_get_pixels(out);

    /* EV → multiplicative factor (2^ev), plus black point offset */
    double factor = pow(2.0, st->ev);
    double black_off = st->black * 1.28;   /* 0..128 */

    /* LUT */
    guchar lut[256];
    for (int i = 0; i < 256; i++) {
        double v = i;
        /* Apply black point lift */
        v = v * (1.0 - st->black / 100.0) + black_off;
        /* Apply EV */
        v *= factor;
        if (v < 0) v = 0;
        if (v > 255) v = 255;
        lut[i] = (guchar)(v + 0.5);
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

static void regenerate(ExpoState *st) {
    if (!st->original) return;
    if (fabs(st->ev) < 0.01 && st->black == 0) {
        g_clear_object(&st->preview);
        update_preview(st);
        return;
    }
    g_clear_object(&st->preview);
    st->preview = apply_exposure(st);
    update_preview(st);
}

/* ------------------------------------------------------------------ */
/* Undo                                                               */
/* ------------------------------------------------------------------ */

static ExpoSnapshot expo_snapshot(ExpoState *st) {
    return (ExpoSnapshot){ st->ev, st->black };
}

static void push_snapshot(ExpoState *st) {
    ExpoSnapshot *s = g_new0(ExpoSnapshot, 1);
    *s = expo_snapshot(st);
    g_ptr_array_add(st->undo_stack, s);
    gtk_widget_set_sensitive(st->undo_btn, TRUE);
}

static void restore_snapshot(ExpoState *st, ExpoSnapshot s) {
    st->ev = s.ev;
    st->black = s.black;
    gtk_range_set_value(GTK_RANGE(st->ev_scale), s.ev);
    gtk_range_set_value(GTK_RANGE(st->black_scale), s.black);
    char buf[32];
    snprintf(buf, sizeof buf, "%+.2f EV", s.ev);
    gtk_label_set_text(GTK_LABEL(st->ev_lbl), buf);
    snprintf(buf, sizeof buf, "%d", s.black);
    gtk_label_set_text(GTK_LABEL(st->black_lbl), buf);
    regenerate(st);
}

/* ------------------------------------------------------------------ */
/* Slider callbacks                                                   */
/* ------------------------------------------------------------------ */

static void on_ev_changed(GtkRange *r, gpointer d) {
    ExpoState *st = get_state(d);
    double new_ev = gtk_range_get_value(r);
    if (fabs(new_ev - st->ev) < 0.01) return;

    push_snapshot(st);
    st->ev = new_ev;

    char buf[32];
    snprintf(buf, sizeof buf, "%+.2f EV", st->ev);
    gtk_label_set_text(GTK_LABEL(st->ev_lbl), buf);

    regenerate(st);
}

static void on_black_changed(GtkRange *r, gpointer d) {
    ExpoState *st = get_state(d);
    int new_black = (int)gtk_range_get_value(r);
    if (new_black == st->black) return;

    push_snapshot(st);
    st->black = new_black;

    char buf[16];
    snprintf(buf, sizeof buf, "%d", st->black);
    gtk_label_set_text(GTK_LABEL(st->black_lbl), buf);

    regenerate(st);
}

/* ------------------------------------------------------------------ */
/* Undo / Reset                                                       */
/* ------------------------------------------------------------------ */

static void on_undo(GtkButton *b, gpointer d) {
    (void)b;
    ExpoState *st = get_state(d);
    if (st->undo_stack->len == 0) return;
    ExpoSnapshot *s = g_ptr_array_index(st->undo_stack,
                                          st->undo_stack->len - 1);
    g_ptr_array_remove_index(st->undo_stack, st->undo_stack->len - 1);
    restore_snapshot(st, *s);
    g_free(s);
    gtk_widget_set_sensitive(st->undo_btn, st->undo_stack->len > 0);
}

static void on_reset(GtkButton *b, gpointer d) {
    (void)b;
    ExpoState *st = get_state(d);
    push_snapshot(st);
    restore_snapshot(st, (ExpoSnapshot){ 0.0, 0 });
}

/* ------------------------------------------------------------------ */
/* Save / Load                                                        */
/* ------------------------------------------------------------------ */

static void on_save(GtkButton *b, gpointer d) {
    (void)b;
    ExpoState *st = get_state(d);
    GdkPixbuf *src = st->preview ? st->preview : st->original;
    if (!src) return;
    char *base = st->path ? g_path_get_basename(st->path) : g_strdup("image.png");
    char *name = g_strdup_printf("exposure_%s", base);
    image_save_pixbuf_dialog(st->root, src, name);
    g_free(name);
    g_free(base);
}

static void on_drop(const char *path, gpointer d) {
    ExpoState *st = get_state(d);
    GError *e = NULL;
    GdkPixbuf *pb = gdk_pixbuf_new_from_file(path, &e);
    if (!pb) { image_show_error(st->root, e->message); g_error_free(e); return; }

    g_clear_object(&st->original);
    g_clear_object(&st->preview);
    g_clear_object(&st->first_original);
    g_free(st->path);

    for (guint i = 0; i < st->undo_stack->len; i++)
        g_free(g_ptr_array_index(st->undo_stack, i));
    g_ptr_array_set_size(st->undo_stack, 0);
    gtk_widget_set_sensitive(st->undo_btn, FALSE);

    st->original = pb;
    st->first_original = gdk_pixbuf_copy(pb);
    st->path = g_strdup(path);
    st->ev = 0;
    st->black = 0;

    gtk_range_set_value(GTK_RANGE(st->ev_scale), 0);
    gtk_range_set_value(GTK_RANGE(st->black_scale), 0);
    gtk_label_set_text(GTK_LABEL(st->ev_lbl), "+0.00 EV");
    gtk_label_set_text(GTK_LABEL(st->black_lbl), "0");

    update_preview(st);
    gtk_stack_set_visible_child_name(GTK_STACK(st->stack), "editor");
}

void image_exposure_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "expo-state", NULL);
}

static void expo_state_free(ExpoState *st) {
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

/* ------------------------------------------------------------------ */
/* Commands                                                           */
/* ------------------------------------------------------------------ */

static void cmd_reset(GtkWidget *v) {
    ExpoState *st = get_state(v);
    push_snapshot(st);
    restore_snapshot(st, (ExpoSnapshot){ 0.0, 0 });
}

const HelvetiaToolCommand image_exposure_commands[] = {
    { .id = "reset", .name = "Reset",
      .icon_name = "view-refresh-symbolic",
      .accel = NULL, .tooltip = "Reset exposure",
      .activate = cmd_reset },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

/* ------------------------------------------------------------------ */
/* Create                                                             */
/* ------------------------------------------------------------------ */

static GtkWidget *make_slider_row(const char *label,
                                    double min, double max, double step,
                                    double initial,
                                    GtkWidget **out_scale,
                                    GtkWidget **out_value_lbl,
                                    GCallback cb, gpointer user_data) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

    GtkWidget *lbl = gtk_label_new(label);
    gtk_widget_add_css_class(lbl, "dim-label");
    gtk_widget_set_size_request(lbl, 80, -1);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);

    GtkWidget *scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,
                                                 min, max, step);
    gtk_widget_set_size_request(scale, 220, -1);
    gtk_range_set_value(GTK_RANGE(scale), initial);
    gtk_scale_set_draw_value(GTK_SCALE(scale), FALSE);
    gtk_widget_set_hexpand(scale, TRUE);

    GtkWidget *val = gtk_label_new("");
    gtk_widget_set_size_request(val, 70, -1);
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

GtkWidget *image_exposure_create(void) {
    ExpoState *st = g_new0(ExpoState, 1);
    st->undo_stack = g_ptr_array_new();

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;
    image_install_edit_shortcuts(root,
        (ImageToolCallback)on_undo,
        (ImageToolCallback)on_reset,
        root);

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
        image_build_drop_zone("Image file", on_drop, root));
    gtk_stack_add_named(GTK_STACK(stack), drop_box, "drop");

    /* Editor */
    GtkWidget *editor = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    GtkWidget *sliders = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start(sliders, 12);
    gtk_widget_set_margin_end(sliders, 12);
    gtk_widget_set_margin_top(sliders, 8);
    gtk_widget_set_margin_bottom(sliders, 8);

    gtk_box_append(GTK_BOX(sliders),
        make_slider_row("Exposure", -3.0, 3.0, 0.01, 0.0,
                        &st->ev_scale, &st->ev_lbl,
                        G_CALLBACK(on_ev_changed), root));
    gtk_box_append(GTK_BOX(sliders),
        make_slider_row("Black point", 0, 100, 1, 0,
                        &st->black_scale, &st->black_lbl,
                        G_CALLBACK(on_black_changed), root));
    gtk_label_set_text(GTK_LABEL(st->ev_lbl), "+0.00 EV");
    gtk_label_set_text(GTK_LABEL(st->black_lbl), "0");

    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(bar, 12);
    gtk_widget_set_margin_end(bar, 12);
    gtk_widget_set_margin_bottom(bar, 8);

    st->undo_btn = image_undo_button(G_CALLBACK(on_undo), root);
    gtk_widget_set_sensitive(st->undo_btn, FALSE);
    GtkWidget *reset = image_reset_button(G_CALLBACK(on_reset), root);

    GtkWidget *sp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(sp, TRUE);

    GtkWidget *save = gtk_button_new_with_label("Save As…");
    gtk_widget_add_css_class(save, "flat");

    gtk_box_append(GTK_BOX(bar), st->undo_btn);
    gtk_box_append(GTK_BOX(bar), reset);
    gtk_box_append(GTK_BOX(bar), sp);
    gtk_box_append(GTK_BOX(bar), image_new_image_button(on_drop, root));
    gtk_box_append(GTK_BOX(bar), gtk_separator_new(GTK_ORIENTATION_VERTICAL));
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
    gtk_box_append(GTK_BOX(root), stack);

    g_object_set_data_full(G_OBJECT(root), "expo-state", st,
                           (GDestroyNotify)expo_state_free);

    g_signal_connect(save, "clicked", G_CALLBACK(on_save), root);

    return root;
}
