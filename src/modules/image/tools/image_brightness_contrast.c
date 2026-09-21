#include <gtk/gtk.h>
#include <adwaita.h>
#include <math.h>
#include "../image_shared.h"
#include "image_brightness_contrast.h"

/* ------------------------------------------------------------------ */
/* State                                                              */
/* ------------------------------------------------------------------ */

typedef struct {
    GdkPixbuf *original;    /* loaded from disk, never modified */
    GdkPixbuf *preview;     /* current adjusted copy */
    char      *path;

    double     brightness;  /* -100..+100 */
    double     contrast;    /* -100..+100 */

    GtkWidget *stack;
    GtkWidget *picture;
    GtkWidget *root;
    GtkWidget *brightness_scale;
    GtkWidget *brightness_lbl;
    GtkWidget *contrast_scale;
    GtkWidget *contrast_lbl;
    GtkWidget *reset_btn;
    GtkWidget *save_btn;

    GdkPixbuf *first_original;
    GPtrArray *undo_stack;
    GtkWidget *undo_btn;
} BCState;

static BCState *get_state(GtkWidget *v) {
    return g_object_get_data(G_OBJECT(v), "bc-state");
}

/* ------------------------------------------------------------------ */
/* Preview rendering                                                  */
/* ------------------------------------------------------------------ */

static void update_preview(BCState *st) {
    GdkPixbuf *src = st->preview ? st->preview : st->original;
    if (!src) return;
    GdkTexture *t = gdk_texture_new_for_pixbuf(src);
    gtk_picture_set_paintable(GTK_PICTURE(st->picture), GDK_PAINTABLE(t));
    gtk_picture_set_content_fit(GTK_PICTURE(st->picture),
                                 GTK_CONTENT_FIT_CONTAIN);
    gtk_picture_set_can_shrink(GTK_PICTURE(st->picture), TRUE);
    g_object_unref(t);
}

/* ------------------------------------------------------------------ */
/* Apply brightness + contrast                                        */
/* ------------------------------------------------------------------ */

/*
 * Formula:
 *   brightness: offset added directly to each channel (-255..255 range internally,
 *               but our slider is -100..+100 → mapped to ±128 offset)
 *   contrast:   factor around midpoint 128
 *               contrast = -100 → factor 0.0   (gray)
 *               contrast =    0 → factor 1.0   (no change)
 *               contrast = +100 → factor 2.0   (extreme)
 */

static GdkPixbuf *apply_bc(BCState *st) {
    GdkPixbuf *src = st->original;
    int w = gdk_pixbuf_get_width(src);
    int h = gdk_pixbuf_get_height(src);
    int n = gdk_pixbuf_get_n_channels(src);
    int stride = gdk_pixbuf_get_rowstride(src);
    guchar *px = gdk_pixbuf_get_pixels(src);

    GdkPixbuf *out = gdk_pixbuf_copy(src);
    int ostride = gdk_pixbuf_get_rowstride(out);
    guchar *opx = gdk_pixbuf_get_pixels(out);

    /* Map slider values to actual adjustments */
    double bright_off = st->brightness * 1.28;   /* -128..+128 */
    double contrast_f = 1.0;
    if (st->contrast >= 0)
        contrast_f = 1.0 + (st->contrast / 100.0);        /* 1.0..2.0 */
    else
        contrast_f = 1.0 + (st->contrast / 100.0);        /* 0.0..1.0 */

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            guchar *sp = px + y * stride + x * n;
            guchar *dp = opx + y * ostride + x * n;

            for (int c = 0; c < 3; c++) {   /* skip alpha */
                double v = sp[c];
                /* Apply contrast around midpoint 128 */
                v = (v - 128.0) * contrast_f + 128.0;
                /* Apply brightness offset */
                v += bright_off;
                /* Clamp */
                if (v < 0) v = 0;
                if (v > 255) v = 255;
                dp[c] = (guchar)(v + 0.5);
            }
            /* Copy alpha unchanged */
            if (n == 4) dp[3] = sp[3];
        }
    }
    return out;
}

/* ------------------------------------------------------------------ */
/* Regenerate preview (called when sliders change or reset)           */
/* ------------------------------------------------------------------ */

static void regenerate(BCState *st) {
    if (!st->original) return;

    /* If both are zero, show original (no work needed) */
    if (fabs(st->brightness) < 0.01 && fabs(st->contrast) < 0.01) {
        g_clear_object(&st->preview);
        update_preview(st);
        return;
    }

    g_clear_object(&st->preview);
    st->preview = apply_bc(st);
    update_preview(st);
}

/* ------------------------------------------------------------------ */
/* Slider callbacks                                                   */
/* ------------------------------------------------------------------ */


typedef struct {
    double brightness, contrast;
} BCSnapshot;

static BCSnapshot bc_snapshot(BCState *st) {
    return (BCSnapshot){ .brightness = st->brightness, .contrast = st->contrast };
}

static void bc_restore(BCState *st, BCSnapshot s) {
    st->brightness = s.brightness;
    st->contrast = s.contrast;

    gtk_range_set_value(GTK_RANGE(st->brightness_scale), s.brightness);
    gtk_range_set_value(GTK_RANGE(st->contrast_scale), s.contrast);

    char buf[32];
    snprintf(buf, sizeof buf, "%+.0f", s.brightness);
    gtk_label_set_text(GTK_LABEL(st->brightness_lbl), buf);

    snprintf(buf, sizeof buf, "%+.0f", s.contrast);
    gtk_label_set_text(GTK_LABEL(st->contrast_lbl), buf);

    regenerate(st);
}

static void push_snapshot(BCState *st) {
    BCSnapshot *s = g_new0(BCSnapshot, 1);
    *s = bc_snapshot(st);
    g_ptr_array_add(st->undo_stack, s);
    gtk_widget_set_sensitive(st->undo_btn, TRUE);
}

static void on_brightness_changed(GtkRange *r, gpointer d) {
    BCState *st = get_state(d);
    double new_val = gtk_range_get_value(r);
    if (fabs(new_val - st->brightness) < 0.001) return;
    push_snapshot(st);
    st->brightness = new_val;

    char buf[32];
    snprintf(buf, sizeof buf, "%+.0f", st->brightness);
    gtk_label_set_text(GTK_LABEL(st->brightness_lbl), buf);

    regenerate(st);
}

static void on_contrast_changed(GtkRange *r, gpointer d) {
    BCState *st = get_state(d);
    double new_val = gtk_range_get_value(r);
    if (fabs(new_val - st->contrast) < 0.001) return;
    push_snapshot(st);
    st->contrast = new_val;

    char buf[32];
    snprintf(buf, sizeof buf, "%+.0f", st->contrast);
    gtk_label_set_text(GTK_LABEL(st->contrast_lbl), buf);

    regenerate(st);
}

/* ------------------------------------------------------------------ */
/* Reset                                                              */
/* ------------------------------------------------------------------ */

static void on_undo(GtkButton *b, gpointer d) {
    (void)b;
    BCState *st = get_state(d);
    if (st->undo_stack->len == 0) return;

    BCSnapshot *s = g_ptr_array_index(st->undo_stack, st->undo_stack->len - 1);
    g_ptr_array_remove_index(st->undo_stack, st->undo_stack->len - 1);

    bc_restore(st, *s);
    g_free(s);

    gtk_widget_set_sensitive(st->undo_btn, st->undo_stack->len > 0);
}

static void on_reset(GtkButton *b, gpointer d) {
    (void)b;
    BCState *st = get_state(d);
    if (!st->original) return;

    push_snapshot(st);

    st->brightness = 0;
    st->contrast = 0;
    gtk_range_set_value(GTK_RANGE(st->brightness_scale), 0);
    gtk_range_set_value(GTK_RANGE(st->contrast_scale), 0);
    gtk_label_set_text(GTK_LABEL(st->brightness_lbl), "+0");
    gtk_label_set_text(GTK_LABEL(st->contrast_lbl), "+0");
    regenerate(st);
}

/* ------------------------------------------------------------------ */
/* Save                                                               */
/* ------------------------------------------------------------------ */

static void on_save(GtkButton *b, gpointer d) {
    (void)b;
    BCState *st = get_state(d);
    GdkPixbuf *src = st->preview ? st->preview : st->original;
    if (!src) return;

    char *base = st->path ? g_path_get_basename(st->path) : g_strdup("image.png");
    char *name = g_strdup_printf("adjusted_%s", base);
    image_save_pixbuf_dialog(st->root, src, name);
    g_free(name);
    g_free(base);
}

/* ------------------------------------------------------------------ */
/* Load                                                               */
/* ------------------------------------------------------------------ */

static void on_drop(const char *path, gpointer d) {
    BCState *st = get_state(d);

    GError *e = NULL;
    GdkPixbuf *pb = gdk_pixbuf_new_from_file(path, &e);
    if (!pb) {
        image_show_error(st->root, e->message);
        g_error_free(e);
        return;
    }

    g_clear_object(&st->original);
    g_clear_object(&st->preview);
    g_clear_object(&st->first_original);
    g_free(st->path);

    st->original = pb;
    st->first_original = g_object_ref(pb);
    st->path = g_strdup(path);

    if (st->undo_stack) {
        for (guint i = 0; i < st->undo_stack->len; i++)
            g_free(g_ptr_array_index(st->undo_stack, i));
        g_ptr_array_set_size(st->undo_stack, 0);
    }
    if (st->undo_btn) gtk_widget_set_sensitive(st->undo_btn, FALSE);
    st->brightness = 0;
    st->contrast = 0;

    gtk_range_set_value(GTK_RANGE(st->brightness_scale), 0);
    gtk_range_set_value(GTK_RANGE(st->contrast_scale), 0);
    gtk_label_set_text(GTK_LABEL(st->brightness_lbl), "+0");
    gtk_label_set_text(GTK_LABEL(st->contrast_lbl), "+0");

    update_preview(st);
    gtk_stack_set_visible_child_name(GTK_STACK(st->stack), "editor");
}

/* ------------------------------------------------------------------ */
/* Lifecycle                                                          */
/* ------------------------------------------------------------------ */

static void bc_state_free(BCState *st) {
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

void image_brightness_contrast_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "bc-state", NULL);
}

/* ------------------------------------------------------------------ */
/* Commands                                                           */
/* ------------------------------------------------------------------ */

static void cmd_reset(GtkWidget *v) {
    BCState *st = get_state(v);
    st->brightness = 0;
    st->contrast = 0;
    gtk_range_set_value(GTK_RANGE(st->brightness_scale), 0);
    gtk_range_set_value(GTK_RANGE(st->contrast_scale), 0);
    regenerate(st);
}

const HelvetiaToolCommand image_brightness_contrast_commands[] = {
    { .id = "reset", .name = "Reset",
      .icon_name = "view-refresh-symbolic",
      .accel = NULL, .tooltip = "Reset brightness and contrast",
      .activate = cmd_reset },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

/* ------------------------------------------------------------------ */
/* Create                                                             */
/* ------------------------------------------------------------------ */

GtkWidget *image_brightness_contrast_create(void) {
    BCState *st = g_new0(BCState, 1);
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

    /* ---- Drop page ---- */
    GtkWidget *drop_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(drop_box, 24);
    gtk_widget_set_margin_end(drop_box, 24);
    gtk_widget_set_margin_top(drop_box, 24);
    gtk_widget_set_margin_bottom(drop_box, 24);
    gtk_widget_set_vexpand(drop_box, TRUE);
    gtk_box_append(GTK_BOX(drop_box),
        image_build_drop_zone("Image file", on_drop, root));
    gtk_stack_add_named(GTK_STACK(stack), drop_box, "drop");

    /* ---- Editor page ---- */
    GtkWidget *editor = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_start(bar, 12);
    gtk_widget_set_margin_end(bar, 12);
    gtk_widget_set_margin_top(bar, 8);
    gtk_widget_set_margin_bottom(bar, 8);

    /* Brightness */
    GtkWidget *br_lbl = gtk_label_new("Brightness");
    gtk_widget_add_css_class(br_lbl, "dim-label");
    gtk_widget_set_size_request(br_lbl, 80, -1);
    gtk_label_set_xalign(GTK_LABEL(br_lbl), 0.0f);

    st->brightness_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,
                                                     -100, 100, 1);
    gtk_widget_set_size_request(st->brightness_scale, 200, -1);
    gtk_range_set_value(GTK_RANGE(st->brightness_scale), 0);
    gtk_scale_set_draw_value(GTK_SCALE(st->brightness_scale), FALSE);

    st->brightness_lbl = gtk_label_new("+0");
    gtk_widget_set_size_request(st->brightness_lbl, 44, -1);
    gtk_label_set_xalign(GTK_LABEL(st->brightness_lbl), 1.0f);
    gtk_widget_add_css_class(st->brightness_lbl, "dim-label");

    /* Contrast */
    GtkWidget *ct_lbl = gtk_label_new("Contrast");
    gtk_widget_add_css_class(ct_lbl, "dim-label");
    gtk_widget_set_size_request(ct_lbl, 80, -1);
    gtk_label_set_xalign(GTK_LABEL(ct_lbl), 0.0f);

    st->contrast_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,
                                                   -100, 100, 1);
    gtk_widget_set_size_request(st->contrast_scale, 200, -1);
    gtk_range_set_value(GTK_RANGE(st->contrast_scale), 0);
    gtk_scale_set_draw_value(GTK_SCALE(st->contrast_scale), FALSE);

    st->contrast_lbl = gtk_label_new("+0");
    gtk_widget_set_size_request(st->contrast_lbl, 44, -1);
    gtk_label_set_xalign(GTK_LABEL(st->contrast_lbl), 1.0f);
    gtk_widget_add_css_class(st->contrast_lbl, "dim-label");

    /* Buttons */
    GtkWidget *sp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(sp, TRUE);

    st->undo_btn = image_undo_button(G_CALLBACK(on_undo), root);
    gtk_widget_set_sensitive(st->undo_btn, FALSE);

    st->reset_btn = image_reset_button(G_CALLBACK(on_reset), root);

    st->save_btn = gtk_button_new_with_label("Save As…");
    gtk_widget_add_css_class(st->save_btn, "flat");

    /* Compose toolbar: two rows to fit narrow windows */
    GtkWidget *bar_row1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_append(GTK_BOX(bar_row1), br_lbl);
    gtk_box_append(GTK_BOX(bar_row1), st->brightness_scale);
    gtk_box_append(GTK_BOX(bar_row1), st->brightness_lbl);
    gtk_box_append(GTK_BOX(bar_row1), gtk_separator_new(GTK_ORIENTATION_VERTICAL));
    gtk_box_append(GTK_BOX(bar_row1), ct_lbl);
    gtk_box_append(GTK_BOX(bar_row1), st->contrast_scale);
    gtk_box_append(GTK_BOX(bar_row1), st->contrast_lbl);

    gtk_box_append(GTK_BOX(bar), bar_row1);
    gtk_box_append(GTK_BOX(bar), sp);
    gtk_box_append(GTK_BOX(bar), image_new_image_button(on_drop, root));
    gtk_box_append(GTK_BOX(bar), st->undo_btn);
    gtk_box_append(GTK_BOX(bar), st->reset_btn);
    gtk_box_append(GTK_BOX(bar), st->save_btn);

    /* Picture */
    GtkWidget *pic = gtk_picture_new();
    gtk_widget_set_vexpand(pic, TRUE);
    gtk_widget_set_halign(pic, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(pic, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class(pic, "image-viewer-canvas");
    st->picture = pic;

    gtk_box_append(GTK_BOX(editor), bar);
    gtk_box_append(GTK_BOX(editor),
                   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(editor), pic);
    gtk_stack_add_named(GTK_STACK(stack), editor, "editor");

    gtk_stack_set_visible_child_name(GTK_STACK(stack), "drop");
    gtk_box_append(GTK_BOX(root), stack);

    g_object_set_data_full(G_OBJECT(root), "bc-state", st, (GDestroyNotify)bc_state_free);

    g_signal_connect(st->brightness_scale, "value-changed",
                     G_CALLBACK(on_brightness_changed), root);
    g_signal_connect(st->contrast_scale, "value-changed",
                     G_CALLBACK(on_contrast_changed), root);
    
    g_signal_connect(st->save_btn, "clicked", G_CALLBACK(on_save), root);

    return root;
}
