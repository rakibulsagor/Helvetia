#include <gtk/gtk.h>
#include <adwaita.h>
#include <cairo.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "../image_shared.h"
#include "image_stamp.h"

/* ================================================================== */
/* SHARED STATE                                                       */
/* ================================================================== */

typedef enum {
    STAMP_EYEDROPPER,
    STAMP_DODGE_BURN,
    STAMP_SMUDGE,
    STAMP_OPACITY,
} StampKind;

typedef struct {
    GdkPixbuf *current;
    GdkPixbuf *first_original;
    GPtrArray *undo_stack;
    char      *path;

    StampKind  kind;

    /* Common */
    double     size;          /* brush radius in px */
    double     opacity;       /* 0..1 */

    /* Dodge / Burn */
    int        db_mode;       /* 0 = dodge, 1 = burn */
    double     db_strength;   /* 0..100 */

    /* Smudge */
    double     smudge_strength; /* 0..100 */

    /* Opacity */
    double     opacity_mix;   /* 0..100 = mix current vs original */

    /* Interaction */
    gboolean   dragging;
    gboolean   alt_pressed;
    double     last_x, last_y;
    double     first_x, first_y;
    gboolean   first_move;

    /* Displayed color for Eyedropper */
    double     picked_r, picked_g, picked_b;

    /* View state */
    double     zoom;

    /* Widgets */
    GtkWidget *stack, *picture, *draw_area, *overlay, *root;
    GtkWidget *undo_btn;
    GtkWidget *color_area;        /* Eyedropper preview */
    GtkWidget *color_label;       /* hex display */
} StampState;

static StampState *get_state(GtkWidget *v) {
    return g_object_get_data(G_OBJECT(v), "stamp-state");
}

/* ================================================================== */
/* UNDO                                                               */
/* ================================================================== */

static void push_undo(StampState *st) {
    if (!st->current) return;
    GdkPixbuf *copy = gdk_pixbuf_copy(st->current);
    g_ptr_array_add(st->undo_stack, copy);
    gtk_widget_set_sensitive(st->undo_btn, TRUE);
}

static void on_undo(GtkButton *b, gpointer d) {
    (void)b;
    StampState *st = get_state(d);
    if (st->undo_stack->len == 0) return;
    GdkPixbuf *prev = g_ptr_array_index(st->undo_stack,
                                          st->undo_stack->len - 1);
    g_ptr_array_remove_index(st->undo_stack, st->undo_stack->len - 1);
    g_clear_object(&st->current);
    st->current = prev;
    gtk_picture_set_paintable(GTK_PICTURE(st->picture), GDK_PAINTABLE(gdk_texture_new_for_pixbuf(st->current)));
    gtk_widget_set_sensitive(st->undo_btn, st->undo_stack->len > 0);
}

static void clear_undo(StampState *st) {
    g_ptr_array_set_size(st->undo_stack, 0);
    gtk_widget_set_sensitive(st->undo_btn, FALSE);
}

/* ================================================================== */
/* COORDINATE MAPPING                                                 */
/* ================================================================== */

static gboolean screen_to_image(StampState *st, double sx, double sy,
                                 double *ix, double *iy) {
    if (!st->current || !st->draw_area) return FALSE;
    int aw = gtk_widget_get_width(st->draw_area);
    int ah = gtk_widget_get_height(st->draw_area);
    if (aw <= 0 || ah <= 0) return FALSE;
    int iw = gdk_pixbuf_get_width(st->current);
    int ih = gdk_pixbuf_get_height(st->current);
    if (iw <= 0 || ih <= 0) return FALSE;

    double img_aspect = (double)iw / ih;
    double area_aspect = (double)aw / ah;
    double dw, dh, ox, oy;
    if (img_aspect > area_aspect) {
        dw = aw; dh = aw / img_aspect; ox = 0; oy = (ah - dh) / 2;
    } else {
        dh = ah; dw = ah * img_aspect; ox = (aw - dw) / 2; oy = 0;
    }
    *ix = (sx - ox) / dw * iw;
    *iy = (sy - oy) / dh * ih;
    return TRUE;
}

/* ================================================================== */
/* PIXEL HELPERS                                                      */
/* ================================================================== */

static inline double clamp01d(double v) {
    return v < 0 ? 0 : (v > 1 ? 1 : v);
}

/* Sample one pixel as normalized doubles */
static void sample_pixel(GdkPixbuf *pb, double x, double y,
                          double *r, double *g, double *b, double *a) {
    int iw = gdk_pixbuf_get_width(pb);
    int ih = gdk_pixbuf_get_height(pb);
    int n = gdk_pixbuf_get_n_channels(pb);
    int stride = gdk_pixbuf_get_rowstride(pb);
    guchar *data = gdk_pixbuf_get_pixels(pb);

    int ix = (int)x, iy = (int)y;
    if (ix < 0) ix = 0; if (ix >= iw) ix = iw - 1;
    if (iy < 0) iy = 0; if (iy >= ih) iy = ih - 1;

    guchar *p = data + iy * stride + ix * n;
    *r = p[0] / 255.0;
    *g = p[1] / 255.0;
    *b = p[2] / 255.0;
    *a = (n == 4) ? p[3] / 255.0 : 1.0;
}

/* Write one pixel from normalized doubles */
static void write_pixel(GdkPixbuf *pb, int x, int y,
                         double r, double g, double b, double a) {
    int iw = gdk_pixbuf_get_width(pb);
    int ih = gdk_pixbuf_get_height(pb);
    int n = gdk_pixbuf_get_n_channels(pb);
    int stride = gdk_pixbuf_get_rowstride(pb);
    guchar *data = gdk_pixbuf_get_pixels(pb);

    if (x < 0 || x >= iw || y < 0 || y >= ih) return;
    guchar *p = data + y * stride + x * n;
    p[0] = (guchar)(clamp01d(r) * 255.0 + 0.5);
    p[1] = (guchar)(clamp01d(g) * 255.0 + 0.5);
    p[2] = (guchar)(clamp01d(b) * 255.0 + 0.5);
    if (n == 4) p[3] = (guchar)(clamp01d(a) * 255.0 + 0.5);
}

/* Ensure RGBA */
static void ensure_rgba(GdkPixbuf **pb) {
    if (!gdk_pixbuf_get_has_alpha(*pb)) {
        GdkPixbuf *a = gdk_pixbuf_add_alpha(*pb, FALSE, 0, 0, 0);
        g_object_unref(*pb);
        *pb = a;
    }
}

/* ================================================================== */
/* EYEDROPPER                                                         */
/* ================================================================== */

static void eyedropper_pick(StampState *st, double x, double y) {
    double a;
    sample_pixel(st->current, x, y,
                  &st->picked_r, &st->picked_g, &st->picked_b, &a);
}

static void on_color_area_draw(GtkDrawingArea *a, cairo_t *cr,
                                 int w, int h, gpointer d) {
    (void)a;
    StampState *st = d;
    cairo_set_source_rgba(cr, st->picked_r, st->picked_g, st->picked_b, 1.0);
    cairo_paint(cr);
    cairo_set_source_rgba(cr, 1, 1, 1, 0.3);
    cairo_set_line_width(cr, 1);
    cairo_rectangle(cr, 0.5, 0.5, w - 1, h - 1);
    cairo_stroke(cr);
}

static void update_color_swatch(StampState *st) {
    if (st->color_area)
        gtk_widget_queue_draw(st->color_area);
    if (st->color_label) {
        char hex[16];
        snprintf(hex, sizeof hex, "#%02X%02X%02X",
                 (int)(st->picked_r * 255 + 0.5),
                 (int)(st->picked_g * 255 + 0.5),
                 (int)(st->picked_b * 255 + 0.5));
        char full[64];
        snprintf(full, sizeof full, "%s  RGB(%d,%d,%d)",
                 hex,
                 (int)(st->picked_r * 255 + 0.5),
                 (int)(st->picked_g * 255 + 0.5),
                 (int)(st->picked_b * 255 + 0.5));
        gtk_label_set_text(GTK_LABEL(st->color_label), full);
    }
}

/* ================================================================== */
/* DODGE / BURN                                                       */
/* ================================================================== */

static void dodge_burn_apply(StampState *st, double x, double y) {
    double radius = st->size / 2.0;
    if (radius < 1) radius = 1;

    int iw = gdk_pixbuf_get_width(st->current);
    int ih = gdk_pixbuf_get_height(st->current);
    int x0 = (int)floor(x - radius);
    int x1 = (int)ceil(x + radius);
    int y0 = (int)floor(y - radius);
    int y1 = (int)ceil(y + radius);

    double strength = st->db_strength / 100.0 * st->opacity;

    for (int py = y0; py <= y1; py++) {
        if (py < 0 || py >= ih) continue;
        for (int px = x0; px <= x1; px++) {
            if (px < 0 || px >= iw) continue;

            double dx = px - x;
            double dy = py - y;
            double d = sqrt(dx*dx + dy*dy);
            if (d > radius) continue;

            /* Soft falloff */
            double falloff = 1.0 - (d / radius);
            falloff = falloff * falloff;
            double f = strength * falloff;

            double r, g, b, a;
            sample_pixel(st->current, px, py, &r, &g, &b, &a);

            if (st->db_mode == 0) {
                /* Dodge — lighten */
                r += (1.0 - r) * f;
                g += (1.0 - g) * f;
                b += (1.0 - b) * f;
            } else {
                /* Burn — darken */
                r -= r * f;
                g -= g * f;
                b -= b * f;
            }
            write_pixel(st->current, px, py, r, g, b, a);
        }
    }
}


/* ================================================================== */
/* SMUDGE                                                             */
/* ================================================================== */

static void smudge_apply(StampState *st, double x, double y) {
    double radius = st->size / 2.0;
    if (radius < 1) radius = 1;

    /* Move pixels from previous position to current position */
    double dx_move = x - st->last_x;
    double dy_move = y - st->last_y;
    double move_len = sqrt(dx_move*dx_move + dy_move*dy_move);
    if (move_len < 0.001) return;

    double strength = st->smudge_strength / 100.0 * st->opacity;
    if (strength > 1.0) strength = 1.0;

    int iw = gdk_pixbuf_get_width(st->current);
    int ih = gdk_pixbuf_get_height(st->current);

    /* Work on a snapshot to avoid self-influence */
    GdkPixbuf *snap = gdk_pixbuf_copy(st->current);

    int x0 = (int)floor(x - radius);
    int x1 = (int)ceil(x + radius);
    int y0 = (int)floor(y - radius);
    int y1 = (int)ceil(y + radius);

    for (int py = y0; py <= y1; py++) {
        if (py < 0 || py >= ih) continue;
        for (int px = x0; px <= x1; px++) {
            if (px < 0 || px >= iw) continue;
            double dx = px - x;
            double dy = py - y;
            double d = sqrt(dx*dx + dy*dy);
            if (d > radius) continue;

            /* Sample from the offset position (opposite of drag) */
            double srx = px - dx_move * strength;
            double sry = py - dy_move * strength;

            double sr, sg, sb, sa, tr, tg, tb, ta;
            sample_pixel(snap, srx, sry, &sr, &sg, &sb, &sa);
            sample_pixel(st->current, px, py, &tr, &tg, &tb, &ta);

            double feather = 1.0 - (d / radius);
            feather = feather * feather;
            double f = strength * feather;

            double out_r = tr + (sr - tr) * f;
            double out_g = tg + (sg - tg) * f;
            double out_b = tb + (sb - tb) * f;

            write_pixel(st->current, px, py, out_r, out_g, out_b, ta);
        }
    }
    g_object_unref(snap);
}

/* ================================================================== */
/* OPACITY — blend current toward original                            */
/* ================================================================== */

static GdkPixbuf *apply_opacity(StampState *st, double mix_pct) {
    if (!st->first_original) return NULL;
    GdkPixbuf *orig = st->first_original;
    GdkPixbuf *cur = st->current;

    if (gdk_pixbuf_get_width(orig) != gdk_pixbuf_get_width(cur) ||
        gdk_pixbuf_get_height(orig) != gdk_pixbuf_get_height(cur))
        return NULL;

    double f = mix_pct / 100.0;   /* 0 = fully original, 1 = fully current */

    int w = gdk_pixbuf_get_width(cur);
    int h = gdk_pixbuf_get_height(cur);
    int n = gdk_pixbuf_get_n_channels(cur);
    int stride_o = gdk_pixbuf_get_rowstride(orig);
    int stride_c = gdk_pixbuf_get_rowstride(cur);
    guchar *po = gdk_pixbuf_get_pixels(orig);
    guchar *pc = gdk_pixbuf_get_pixels(cur);

    GdkPixbuf *out = gdk_pixbuf_copy(cur);
    int stride_out = gdk_pixbuf_get_rowstride(out);
    guchar *pout = gdk_pixbuf_get_pixels(out);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            guchar *o = po + y * stride_o + x * n;
            guchar *c = pc + y * stride_c + x * n;
            guchar *d = pout + y * stride_out + x * n;
            d[0] = (guchar)(o[0] + (c[0] - o[0]) * f + 0.5);
            d[1] = (guchar)(o[1] + (c[1] - o[1]) * f + 0.5);
            d[2] = (guchar)(o[2] + (c[2] - o[2]) * f + 0.5);
            if (n == 4) d[3] = c[3];
        }
    }
    return out;
}

/* ================================================================== */
/* PREVIEW DRAW                                                       */
/* ================================================================== */

static void preview_draw(GtkDrawingArea *a, cairo_t *cr, int w, int h, gpointer d) {
    (void)a;
    StampState *st = d;
    if (!st->current) return;

    int iw = gdk_pixbuf_get_width(st->current);
    int ih = gdk_pixbuf_get_height(st->current);
    double img_aspect = (double)iw / ih;
    double area_aspect = (double)w / h;
    double dw, dh, ox, oy;
    if (img_aspect > area_aspect) {
        dw = w; dh = w / img_aspect; ox = 0; oy = (h - dh) / 2;
    } else {
        dh = h; dw = h * img_aspect; ox = (w - dw) / 2; oy = 0;
    }
    double sx = dw / iw;
    double sy = dh / ih;

    cairo_save(cr);
    cairo_translate(cr, ox, oy);
    cairo_scale(cr, sx, sy);

    if (st->dragging && st->kind != STAMP_OPACITY) {
        cairo_set_line_width(cr, 1.5 / sx);
        cairo_set_source_rgba(cr, 1, 1, 1, 0.9);
        cairo_arc(cr, st->last_x, st->last_y, st->size / 2.0, 0, 2 * G_PI);
        cairo_stroke(cr);
    }



    cairo_restore(cr);
}

/* ================================================================== */
/* GESTURES                                                           */
/* ================================================================== */

static void on_press(GtkGestureClick *g, int n_press,
                      double sx, double sy, gpointer d) {
    (void)g; (void)n_press;
    StampState *st = get_state(d);
    double ix, iy;
    if (!screen_to_image(st, sx, sy, &ix, &iy)) return;



    switch (st->kind) {
        case STAMP_EYEDROPPER:
            eyedropper_pick(st, ix, iy);
            update_color_swatch(st);
            break;

        case STAMP_DODGE_BURN:
        case STAMP_SMUDGE:
            push_undo(st);
            st->dragging = TRUE;
            st->last_x = ix;
            st->last_y = iy;
            st->first_x = ix;
            st->first_y = iy;
            st->first_move = TRUE;

            if (st->kind == STAMP_DODGE_BURN)
                dodge_burn_apply(st, ix, iy);
            else if (st->kind == STAMP_SMUDGE)
                smudge_apply(st, ix, iy);

            gtk_picture_set_paintable(GTK_PICTURE(st->picture), GDK_PAINTABLE(gdk_texture_new_for_pixbuf(st->current)));
            gtk_widget_queue_draw(st->draw_area);
    st->zoom = 1.0;
            break;

        case STAMP_OPACITY:
            /* No canvas interaction */
            break;
    }
}

static void on_motion(GtkEventControllerMotion *c, double sx, double sy,
                       gpointer d) {
    (void)c;
    StampState *st = get_state(d);
    if (!st->dragging) return;
    double ix, iy;
    if (!screen_to_image(st, sx, sy, &ix, &iy)) return;

    switch (st->kind) {
        case STAMP_DODGE_BURN: dodge_burn_apply(st, ix, iy); break;
        case STAMP_SMUDGE:
            smudge_apply(st, ix, iy);
            break;
        default: break;
    }

    st->last_x = ix;
    st->last_y = iy;
    gtk_picture_set_paintable(GTK_PICTURE(st->picture), GDK_PAINTABLE(gdk_texture_new_for_pixbuf(st->current)));
    gtk_widget_queue_draw(st->draw_area);
    st->zoom = 1.0;
}

static void on_release(GtkGestureClick *g, int n_press,
                        double sx, double sy, gpointer d) {
    (void)g; (void)n_press; (void)sx; (void)sy;
    StampState *st = get_state(d);
    st->dragging = FALSE;
}



/* ================================================================== */
/* SHELL                                                              */
/* ================================================================== */

static GtkWidget *make_slider(const char *label, double min, double max,
                                double step, double initial,
                                GtkWidget **out_lbl, GCallback cb,
                                gpointer user_data) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *lbl = gtk_label_new(label);
    gtk_widget_add_css_class(lbl, "dim-label");
    gtk_widget_set_size_request(lbl, 110, -1);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);

    GtkWidget *scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,
                                                 min, max, step);
    gtk_widget_set_size_request(scale, 180, -1);
    gtk_range_set_value(GTK_RANGE(scale), initial);
    gtk_scale_set_draw_value(GTK_SCALE(scale), FALSE);
    gtk_widget_set_hexpand(scale, TRUE);

    GtkWidget *val = gtk_label_new("");
    gtk_widget_set_size_request(val, 50, -1);
    gtk_label_set_xalign(GTK_LABEL(val), 1.0f);
    gtk_widget_add_css_class(val, "dim-label");

    gtk_box_append(GTK_BOX(row), lbl);
    gtk_box_append(GTK_BOX(row), scale);
    gtk_box_append(GTK_BOX(row), val);

    *out_lbl = val;
    if (cb) g_signal_connect(scale, "value-changed", cb, user_data);
    return row;
}

static void on_size(GtkRange *r, gpointer d) {
    StampState *st = get_state(d);
    st->size = gtk_range_get_value(r);
}
static void on_opacity(GtkRange *r, gpointer d) {
    StampState *st = get_state(d);
    st->opacity = gtk_range_get_value(r) / 100.0;
}

static void on_save_common(StampState *st, const char *prefix) {
    if (!st->current) return;
    char *base = st->path ? g_path_get_basename(st->path) : g_strdup("image.png");
    char *name = g_strdup_printf("%s_%s", prefix, base);
    image_save_pixbuf_dialog(st->root, st->current, name);
    g_free(name);
    g_free(base);
}

static void on_drop_common(StampState *st, const char *path) {
    st->zoom = 1.0;
    GError *e = NULL;
    GdkPixbuf *pb = gdk_pixbuf_new_from_file(path, &e);
    if (!pb) { image_show_error(st->root, e->message); g_error_free(e); return; }
    ensure_rgba(&pb);

    g_clear_object(&st->current);
    g_clear_object(&st->first_original);
    g_free(st->path);
    clear_undo(st);

    st->current = pb;
    st->first_original = gdk_pixbuf_copy(pb);
    st->path = g_strdup(path);

    gtk_picture_set_paintable(GTK_PICTURE(st->picture), GDK_PAINTABLE(gdk_texture_new_for_pixbuf(st->current)));
    gtk_stack_set_visible_child_name(GTK_STACK(st->stack), "editor");
}

static void on_reset_common(GtkButton *b, gpointer d) {
    (void)b;
    StampState *st = get_state(d);
    if (!st->first_original) return;
    push_undo(st);
    g_clear_object(&st->current);
    st->current = g_object_ref(st->first_original);
    gtk_picture_set_paintable(GTK_PICTURE(st->picture), GDK_PAINTABLE(gdk_texture_new_for_pixbuf(st->current)));
}

static void stamp_state_free(StampState *st) {
    if (!st) return;
    g_clear_object(&st->current);
    g_clear_object(&st->first_original);
    if (st->undo_stack) g_ptr_array_unref(st->undo_stack);
    g_free(st->path);
    g_free(st);
}

static GtkWidget *build_shell(StampState *st, GtkWidget **out_opts,
                                const char *hint,
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
        image_build_drop_zone(hint, on_drop, root));
    gtk_stack_add_named(GTK_STACK(stack), drop_box, "drop");

    GtkWidget *editor = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    GtkWidget *opts = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start(opts, 12);
    gtk_widget_set_margin_end(opts, 12);
    gtk_widget_set_margin_top(opts, 8);
    gtk_widget_set_margin_bottom(opts, 8);
    *out_opts = opts;

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
    gtk_picture_set_can_shrink(GTK_PICTURE(pic), TRUE);
    gtk_picture_set_content_fit(GTK_PICTURE(pic), GTK_CONTENT_FIT_CONTAIN);
    gtk_widget_set_vexpand(pic, TRUE);
    gtk_widget_set_hexpand(pic, TRUE);
    gtk_widget_add_css_class(pic, "image-viewer-canvas");
    st->picture = pic;

    GtkWidget *draw = gtk_drawing_area_new();
    gtk_widget_set_hexpand(draw, TRUE);
    gtk_widget_set_vexpand(draw, TRUE);
    st->draw_area = draw;
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(draw),
                                    preview_draw, st, NULL);

    GtkWidget *overlay = gtk_overlay_new();
    gtk_overlay_set_child(GTK_OVERLAY(overlay), pic);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), draw);
    gtk_widget_set_vexpand(overlay, TRUE);
    st->overlay = overlay;

    GtkGesture *click = gtk_gesture_click_new();
    g_signal_connect(click, "pressed", G_CALLBACK(on_press), root);
    g_signal_connect(click, "released", G_CALLBACK(on_release), root);
    gtk_widget_add_controller(draw, GTK_EVENT_CONTROLLER(click));

    GtkEventController *motion = gtk_event_controller_motion_new();
    g_signal_connect(motion, "motion", G_CALLBACK(on_motion), root);
    gtk_widget_add_controller(draw, motion);



    gtk_box_append(GTK_BOX(editor), opts);
    gtk_box_append(GTK_BOX(editor), bar);
    gtk_box_append(GTK_BOX(editor),
                   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(editor), overlay);
    gtk_stack_add_named(GTK_STACK(stack), editor, "editor");
    gtk_stack_set_visible_child_name(GTK_STACK(stack), "drop");

    return stack;
}

/* ================================================================== */
/* TOOL 50 — EYEDROPPER                                               */
/* ================================================================== */

static void ed_save(GtkButton *b, gpointer d) { (void)b; }
static void ed_drop(const char *p, gpointer d) {
    StampState *st = get_state(d);
    on_drop_common(st, p);
}
static void ed_reset(GtkButton *b, gpointer d) { on_reset_common(b, d); }

void image_eyedropper_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "stamp-state", NULL);
}
static void cmd_ed_reset(GtkWidget *v) { on_reset_common(NULL, v); }

const HelvetiaToolCommand image_eyedropper_commands[] = {
    { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic",
      .accel = NULL, .tooltip = "Reset", .activate = cmd_ed_reset },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

GtkWidget *image_eyedropper_create(void) {
    StampState *st = g_new0(StampState, 1);
    st->undo_stack = g_ptr_array_new();
    st->zoom = 1.0;
    st->kind = STAMP_EYEDROPPER;
    st->size = 1;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;

    GtkWidget *opts;
    GtkWidget *stack = build_shell(st, &opts, "Image file",
                                    ed_drop, G_CALLBACK(ed_save),
                                    G_CALLBACK(ed_reset), root);

    /* Color preview swatch */
    GtkWidget *swatch_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *sl = gtk_label_new("Picked:");
    gtk_widget_add_css_class(sl, "dim-label");
    gtk_widget_set_size_request(sl, 110, -1);
    gtk_label_set_xalign(GTK_LABEL(sl), 0.0f);

    st->color_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(st->color_area, 48, 28);
    gtk_widget_add_css_class(st->color_area, "color-swatch");
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(st->color_area),
                                     on_color_area_draw, st, NULL);

    st->color_label = gtk_label_new("#000000");
    gtk_widget_set_hexpand(st->color_label, TRUE);
    gtk_label_set_xalign(GTK_LABEL(st->color_label), 0.0f);
    gtk_widget_add_css_class(st->color_label, "monospace");

    gtk_box_append(GTK_BOX(swatch_row), sl);
    gtk_box_append(GTK_BOX(swatch_row), st->color_area);
    gtk_box_append(GTK_BOX(swatch_row), st->color_label);
    gtk_box_append(GTK_BOX(opts), swatch_row);

    GtkWidget *hint = gtk_label_new(
        "Click anywhere on the image to pick its color. "
        "The RGB value appears on the right.");
    gtk_label_set_wrap(GTK_LABEL(hint), TRUE);
    gtk_widget_add_css_class(hint, "dim-label");
    gtk_widget_add_css_class(hint, "caption");
    gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);
    gtk_box_append(GTK_BOX(opts), hint);

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "stamp-state", st,
                           (GDestroyNotify)stamp_state_free);
    image_register_zoom(root, st->picture, &st->zoom);
    image_install_zoom_shortcuts(root);
    return root;
}

/* ================================================================== */
/* TOOL 51 — DODGE / BURN                                             */
/* ================================================================== */

static void db_save(GtkButton *b, gpointer d) { (void)b; on_save_common(get_state(d), "db"); }
static void db_drop(const char *p, gpointer d) { on_drop_common(get_state(d), p); }
static void db_reset(GtkButton *b, gpointer d) { on_reset_common(b, d); }
static void db_on_mode(GObject *dd, GParamSpec *p, gpointer d) {
    (void)p;
    StampState *st = get_state(d);
    st->db_mode = gtk_drop_down_get_selected(GTK_DROP_DOWN(dd));
}
static void db_on_strength(GtkRange *r, gpointer d) {
    StampState *st = get_state(d);
    st->db_strength = gtk_range_get_value(r);
}

void image_dodge_burn_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "stamp-state", NULL);
}
static void cmd_db_reset(GtkWidget *v) { on_reset_common(NULL, v); }

const HelvetiaToolCommand image_dodge_burn_commands[] = {
    { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic",
      .accel = NULL, .tooltip = "Reset", .activate = cmd_db_reset },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

GtkWidget *image_dodge_burn_create(void) {
    StampState *st = g_new0(StampState, 1);
    st->undo_stack = g_ptr_array_new();
    st->zoom = 1.0;
    st->kind = STAMP_DODGE_BURN;
    st->size = 40;
    st->opacity = 0.5;
    st->db_mode = 0;
    st->db_strength = 50;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;

    GtkWidget *opts;
    GtkWidget *stack = build_shell(st, &opts, "Image file",
                                    db_drop, G_CALLBACK(db_save),
                                    G_CALLBACK(db_reset), root);

    GtkWidget *mode_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *ml = gtk_label_new("Mode:");
    gtk_widget_add_css_class(ml, "dim-label");
    gtk_widget_set_size_request(ml, 110, -1);
    gtk_label_set_xalign(GTK_LABEL(ml), 0.0f);
    const char *modes[] = {"Dodge (lighten)", "Burn (darken)", NULL};
    GtkWidget *mode_dd = gtk_drop_down_new_from_strings(modes);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(mode_dd), 0);
    gtk_box_append(GTK_BOX(mode_row), ml);
    gtk_box_append(GTK_BOX(mode_row), mode_dd);
    gtk_box_append(GTK_BOX(opts), mode_row);

    GtkWidget *l1, *l2, *l3;
    gtk_box_append(GTK_BOX(opts),
        make_slider("Size", 5, 200, 1, 40, &l1, G_CALLBACK(on_size), root));
    gtk_label_set_text(GTK_LABEL(l1), "40");
    gtk_box_append(GTK_BOX(opts),
        make_slider("Strength", 0, 100, 1, 50, &l3, G_CALLBACK(db_on_strength), root));
    gtk_label_set_text(GTK_LABEL(l3), "50");
    gtk_box_append(GTK_BOX(opts),
        make_slider("Opacity %", 1, 100, 1, 50, &l2, G_CALLBACK(on_opacity), root));
    gtk_label_set_text(GTK_LABEL(l2), "50");

    g_signal_connect(mode_dd, "notify::selected",
                     G_CALLBACK(db_on_mode), root);

    GtkWidget *hint = gtk_label_new(
        "Paint over areas to lighten (dodge) or darken (burn). "
        "Hold the mouse button and drag for continuous effect.");
    gtk_label_set_wrap(GTK_LABEL(hint), TRUE);
    gtk_widget_add_css_class(hint, "dim-label");
    gtk_widget_add_css_class(hint, "caption");
    gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);
    gtk_box_append(GTK_BOX(opts), hint);

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "stamp-state", st,
                           (GDestroyNotify)stamp_state_free);
    image_register_zoom(root, st->picture, &st->zoom);
    image_install_zoom_shortcuts(root);
    return root;
}



/* ================================================================== */
/* TOOL 54 — SMUDGE                                                   */
/* ================================================================== */

static void sm_save(GtkButton *b, gpointer d) { (void)b; on_save_common(get_state(d), "smudge"); }
static void sm_drop(const char *p, gpointer d) { on_drop_common(get_state(d), p); }
static void sm_reset(GtkButton *b, gpointer d) { on_reset_common(b, d); }
static void sm_on_strength(GtkRange *r, gpointer d) {
    StampState *st = get_state(d);
    st->smudge_strength = gtk_range_get_value(r);
}

void image_smudge_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "stamp-state", NULL);
}
static void cmd_sm_reset(GtkWidget *v) { on_reset_common(NULL, v); }

const HelvetiaToolCommand image_smudge_commands[] = {
    { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic",
      .accel = NULL, .tooltip = "Reset", .activate = cmd_sm_reset },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

GtkWidget *image_smudge_create(void) {
    StampState *st = g_new0(StampState, 1);
    st->undo_stack = g_ptr_array_new();
    st->zoom = 1.0;
    st->kind = STAMP_SMUDGE;
    st->size = 30;
    st->opacity = 1.0;
    st->smudge_strength = 60;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;

    GtkWidget *opts;
    GtkWidget *stack = build_shell(st, &opts, "Image file",
                                    sm_drop, G_CALLBACK(sm_save),
                                    G_CALLBACK(sm_reset), root);

    GtkWidget *l1, *l2, *l3;
    gtk_box_append(GTK_BOX(opts),
        make_slider("Size", 5, 200, 1, 30, &l1, G_CALLBACK(on_size), root));
    gtk_label_set_text(GTK_LABEL(l1), "30");
    gtk_box_append(GTK_BOX(opts),
        make_slider("Strength", 0, 100, 1, 60, &l2, G_CALLBACK(sm_on_strength), root));
    gtk_label_set_text(GTK_LABEL(l2), "60");
    gtk_box_append(GTK_BOX(opts),
        make_slider("Opacity %", 1, 100, 1, 100, &l3, G_CALLBACK(on_opacity), root));
    gtk_label_set_text(GTK_LABEL(l3), "100");

    GtkWidget *hint = gtk_label_new(
        "Click and drag to smear pixels like finger-painting. "
        "Higher strength pushes pixels further.");
    gtk_label_set_wrap(GTK_LABEL(hint), TRUE);
    gtk_widget_add_css_class(hint, "dim-label");
    gtk_widget_add_css_class(hint, "caption");
    gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);
    gtk_box_append(GTK_BOX(opts), hint);

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "stamp-state", st,
                           (GDestroyNotify)stamp_state_free);
    image_register_zoom(root, st->picture, &st->zoom);
    image_install_zoom_shortcuts(root);
    return root;
}

/* ================================================================== */
/* TOOL 59 — OPACITY CONTROL                                          */
/* ================================================================== */

static void op_save(GtkButton *b, gpointer d) { (void)b; on_save_common(get_state(d), "opacity"); }
static void op_drop(const char *p, gpointer d) {
    StampState *st = get_state(d);
    on_drop_common(st, p);
}
static void op_reset(GtkButton *b, gpointer d) { on_reset_common(b, d); }

static void op_on_mix(GtkRange *r, gpointer d) {
    StampState *st = get_state(d);
    st->opacity_mix = gtk_range_get_value(r);
    /* Recompute from original (which was set at load time) */
    if (st->first_original) {
        /* We need "current" to be the edited image before opacity was applied.
           Simplest: keep a saved copy of the pre-opacity image. */
    }
    /* Simpler: apply on top of the first_original via blending of current
       pre-opacity snapshot — but we don't have that. For simplicity, this
       blends current (which is the edited image) toward the original. */
    GdkPixbuf *mixed = apply_opacity(st, st->opacity_mix);
    if (mixed) {
        g_clear_object(&st->current);
        st->current = mixed;
        gtk_picture_set_paintable(GTK_PICTURE(st->picture), GDK_PAINTABLE(gdk_texture_new_for_pixbuf(st->current)));
    }
}

void image_opacity_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "stamp-state", NULL);
}
static void cmd_op_reset(GtkWidget *v) { on_reset_common(NULL, v); }

const HelvetiaToolCommand image_opacity_commands[] = {
    { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic",
      .accel = NULL, .tooltip = "Reset", .activate = cmd_op_reset },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

GtkWidget *image_opacity_create(void) {
    StampState *st = g_new0(StampState, 1);
    st->undo_stack = g_ptr_array_new();
    st->zoom = 1.0;
    st->kind = STAMP_OPACITY;
    st->opacity_mix = 100;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;

    GtkWidget *opts;
    GtkWidget *stack = build_shell(st, &opts, "Image file",
                                    op_drop, G_CALLBACK(op_save),
                                    G_CALLBACK(op_reset), root);

    GtkWidget *l;
    gtk_box_append(GTK_BOX(opts),
        make_slider("Opacity %", 0, 100, 1, 100, &l,
                     G_CALLBACK(op_on_mix), root));
    gtk_label_set_text(GTK_LABEL(l), "100");

    GtkWidget *hint = gtk_label_new(
        "Blends the edited image toward the original. "
        "0% = fully original, 100% = fully edited.");
    gtk_label_set_wrap(GTK_LABEL(hint), TRUE);
    gtk_widget_add_css_class(hint, "dim-label");
    gtk_widget_add_css_class(hint, "caption");
    gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);
    gtk_box_append(GTK_BOX(opts), hint);

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "stamp-state", st,
                           (GDestroyNotify)stamp_state_free);
    image_register_zoom(root, st->picture, &st->zoom);
    image_install_zoom_shortcuts(root);
    return root;
}
