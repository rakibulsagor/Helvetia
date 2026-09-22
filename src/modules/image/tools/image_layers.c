
#include "image_layers.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "../image_shared.h"

/* ================================================================== */
/* BLEND MODES                                                        */
/* ================================================================== */

const char *blend_mode_name(BlendMode m) {
    switch (m) {
        case BLEND_NORMAL:     return "Normal";
        case BLEND_MULTIPLY:   return "Multiply";
        case BLEND_SCREEN:     return "Screen";
        case BLEND_OVERLAY:    return "Overlay";
        case BLEND_DARKEN:     return "Darken";
        case BLEND_LIGHTEN:    return "Lighten";
        case BLEND_DIFFERENCE: return "Difference";
        case BLEND_ADD:        return "Add";
        case BLEND_SOFT_LIGHT: return "Soft Light";
        case BLEND_HARD_LIGHT: return "Hard Light";
        default:               return "Normal";
    }
}

static inline double norm255(int v) { return v / 255.0; }
static inline guchar  denorm(double v) {
    if (v < 0) v = 0;
    if (v > 1) v = 1;
    return (guchar)(v * 255.0 + 0.5);
}

guchar blend_channel(BlendMode mode, guchar base, guchar top) {
    double b = norm255(base);
    double t = norm255(top);
    double r;

    switch (mode) {
        case BLEND_NORMAL:     r = t; break;
        case BLEND_MULTIPLY:   r = b * t; break;
        case BLEND_SCREEN:     r = 1.0 - (1.0 - b) * (1.0 - t); break;
        case BLEND_OVERLAY:
            r = (b < 0.5) ? (2.0 * b * t) : (1.0 - 2.0 * (1.0 - b) * (1.0 - t));
            break;
        case BLEND_DARKEN:     r = b < t ? b : t; break;
        case BLEND_LIGHTEN:    r = b > t ? b : t; break;
        case BLEND_DIFFERENCE: r = fabs(b - t); break;
        case BLEND_ADD:        r = b + t; break;
        case BLEND_SOFT_LIGHT: {
            if (t <= 0.5)
                r = b - (1.0 - 2.0 * t) * b * (1.0 - b);
            else
                r = b + (2.0 * t - 1.0) * (
                    (b <= 0.25) ? ((16.0 * b - 12.0) * b + 4.0) * b
                                : sqrt(b));
            break;
        }
        case BLEND_HARD_LIGHT:
            r = (t < 0.5) ? (2.0 * b * t)
                          : (1.0 - 2.0 * (1.0 - b) * (1.0 - t));
            break;
        default: r = t;
    }
    return denorm(r);
}

/* ================================================================== */
/* SELECTION MASK                                                     */
/* ================================================================== */

SelectionMask *selection_new(int w, int h) {
    SelectionMask *s = g_new0(SelectionMask, 1);
    s->width = w;
    s->height = h;
    s->mask = g_malloc0(w * h);
    memset(s->mask, 255, w * h);
    s->active = FALSE;
    strcpy(s->shape, "none");
    return s;
}

void selection_free(SelectionMask *s) {
    if (!s) return;
    g_free(s->mask);
    g_free(s);
}

void selection_reset(SelectionMask *s) {
    if (!s) return;
    memset(s->mask, 255, s->width * s->height);
    s->active = FALSE;
    strcpy(s->shape, "none");
}

void selection_none(SelectionMask *s) {
    if (!s) return;
    memset(s->mask, 0, s->width * s->height);
    s->active = TRUE;
    strcpy(s->shape, "empty");
}

gboolean selection_is_selected(SelectionMask *s, int x, int y) {
    if (!s || !s->active) return TRUE;
    if (x < 0 || x >= s->width || y < 0 || y >= s->height) return FALSE;
    return s->mask[y * s->width + x] > 0;
}

guchar selection_weight(SelectionMask *s, int x, int y) {
    if (!s || !s->active) return 255;
    if (x < 0 || x >= s->width || y < 0 || y >= s->height) return 0;
    return s->mask[y * s->width + x];
}

void selection_fill_rect(SelectionMask *s, int x0, int y0, int x1, int y1) {
    if (!s) return;
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    memset(s->mask, 0, s->width * s->height);
    for (int y = y0; y <= y1; y++) {
        if (y < 0 || y >= s->height) continue;
        for (int x = x0; x <= x1; x++) {
            if (x < 0 || x >= s->width) continue;
            s->mask[y * s->width + x] = 255;
        }
    }
    s->active = TRUE;
    strcpy(s->shape, "rect");
}

void selection_fill_ellipse(SelectionMask *s, int x0, int y0, int x1, int y1) {
    if (!s) return;
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    memset(s->mask, 0, s->width * s->height);
    double cx = (x0 + x1) / 2.0;
    double cy = (y0 + y1) / 2.0;
    double rx = (x1 - x0) / 2.0;
    double ry = (y1 - y0) / 2.0;
    if (rx < 0.5) rx = 0.5;
    if (ry < 0.5) ry = 0.5;

    for (int y = y0; y <= y1; y++) {
        if (y < 0 || y >= s->height) continue;
        for (int x = x0; x <= x1; x++) {
            if (x < 0 || x >= s->width) continue;
            double dx = (x - cx) / rx;
            double dy = (y - cy) / ry;
            if (dx*dx + dy*dy <= 1.0)
                s->mask[y * s->width + x] = 255;
        }
    }
    s->active = TRUE;
    strcpy(s->shape, "ellipse");
}

void selection_fill_polygon(SelectionMask *s, const int *xs, const int *ys, int n) {
    if (!s || n < 3) return;
    memset(s->mask, 0, s->width * s->height);

    int ymin = ys[0], ymax = ys[0];
    for (int i = 1; i < n; i++) {
        if (ys[i] < ymin) ymin = ys[i];
        if (ys[i] > ymax) ymax = ys[i];
    }
    if (ymin < 0) ymin = 0;
    if (ymax >= s->height) ymax = s->height - 1;

    for (int y = ymin; y <= ymax; y++) {
        double xsects[64];
        int n_xs = 0;
        for (int i = 0; i < n; i++) {
            int j = (i + 1) % n;
            int yi = ys[i], yj = ys[j];
            if ((y >= yi && y < yj) || (y >= yj && y < yi)) {
                double t = (double)(y - yi) / (yj - yi);
                double xi = xs[i] + t * (xs[j] - xs[i]);
                if (n_xs < 64) xsects[n_xs++] = xi;
            }
        }
        for (int a = 0; a < n_xs - 1; a++)
            for (int b = a + 1; b < n_xs; b++)
                if (xsects[a] > xsects[b]) {
                    double t = xsects[a];
                    xsects[a] = xsects[b];
                    xsects[b] = t;
                }
        for (int a = 0; a + 1 < n_xs; a += 2) {
            int xa = (int)ceil(xsects[a]);
            int xb = (int)floor(xsects[a+1]);
            for (int x = xa; x <= xb; x++) {
                if (x >= 0 && x < s->width)
                    s->mask[y * s->width + x] = 255;
            }
        }
    }
    s->active = TRUE;
    strcpy(s->shape, "lasso");
}

void selection_flood_from(SelectionMask *s, GdkPixbuf *pb,
                           int sx, int sy, int tolerance) {
    if (!s || !pb) return;
    int w = gdk_pixbuf_get_width(pb);
    int h = gdk_pixbuf_get_height(pb);
    if (sx < 0 || sx >= w || sy < 0 || sy >= h) return;

    int n_ch = gdk_pixbuf_get_n_channels(pb);
    int stride = gdk_pixbuf_get_rowstride(pb);
    guchar *data = gdk_pixbuf_get_pixels(pb);

    if (w != s->width || h != s->height) return;

    guchar *target = data + sy * stride + sx * n_ch;
    int tr = target[0], tg = target[1], tb = target[2];

    int tol2 = tolerance * tolerance * 3;

    memset(s->mask, 0, w * h);
    guchar *visited = g_new0(guchar, w * h);

    GQueue *q = g_queue_new();
    g_queue_push_tail(q, GINT_TO_POINTER(sy * w + sx));

    while (!g_queue_is_empty(q)) {
        int idx = GPOINTER_TO_INT(g_queue_pop_head(q));
        if (visited[idx]) continue;
        visited[idx] = 1;

        int y = idx / w;
        int x = idx % w;
        guchar *p = data + y * stride + x * n_ch;
        int dr = (int)p[0] - tr;
        int dg = (int)p[1] - tg;
        int db = (int)p[2] - tb;
        if (dr*dr + dg*dg + db*db > tol2) continue;

        s->mask[idx] = 255;

        if (x > 0)     g_queue_push_tail(q, GINT_TO_POINTER(idx - 1));
        if (x < w - 1) g_queue_push_tail(q, GINT_TO_POINTER(idx + 1));
        if (y > 0)     g_queue_push_tail(q, GINT_TO_POINTER(idx - w));
        if (y < h - 1) g_queue_push_tail(q, GINT_TO_POINTER(idx + w));
    }

    g_queue_free(q);
    g_free(visited);
    s->active = TRUE;
    strcpy(s->shape, "wand");
}

/* ================================================================== */
/* LAYER                                                              */
/* ================================================================== */

Layer *layer_new(const char *name, GdkPixbuf *pixbuf) {
    Layer *l = g_new0(Layer, 1);
    l->name = g_strdup(name);
    l->visible = TRUE;
    l->opacity = 1.0;
    l->blend = BLEND_NORMAL;

    if (pixbuf && !gdk_pixbuf_get_has_alpha(pixbuf)) {
        l->pixbuf = gdk_pixbuf_add_alpha(pixbuf, FALSE, 0, 0, 0);
    } else {
        l->pixbuf = pixbuf ? g_object_ref(pixbuf) : NULL;
    }
    return l;
}

Layer *layer_new_empty(int w, int h, const char *name) {
    GdkPixbuf *pb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, w, h);
    gdk_pixbuf_fill(pb, 0x00000000);
    Layer *l = layer_new(name, pb);
    g_object_unref(pb);
    return l;
}

void layer_free(Layer *l) {
    if (!l) return;
    g_free(l->name);
    g_clear_object(&l->pixbuf);
    g_free(l->mask);
    g_free(l);
}

GdkPixbuf *layer_ensure_rgba(Layer *l) {
    if (!l || !l->pixbuf) return NULL;
    if (gdk_pixbuf_get_has_alpha(l->pixbuf) &&
        gdk_pixbuf_get_n_channels(l->pixbuf) == 4)
        return l->pixbuf;
    GdkPixbuf *a = gdk_pixbuf_add_alpha(l->pixbuf, FALSE, 0, 0, 0);
    g_object_unref(l->pixbuf);
    l->pixbuf = a;
    return l->pixbuf;
}

/* ================================================================== */
/* LAYER STACK                                                        */
/* ================================================================== */

LayerStack *layer_stack_new(int w, int h) {
    LayerStack *ls = g_new0(LayerStack, 1);
    ls->layers = g_ptr_array_new_with_free_func((GDestroyNotify)layer_free);
    ls->width = w;
    ls->height = h;
    ls->active = -1;
    return ls;
}

void layer_stack_free(LayerStack *ls) {
    if (!ls) return;
    g_ptr_array_unref(ls->layers);
    g_free(ls);
}

void layer_stack_add(LayerStack *ls, Layer *l) {
    g_ptr_array_add(ls->layers, l);
    ls->active = ls->layers->len - 1;
}

void layer_stack_insert(LayerStack *ls, Layer *l, int idx) {
    if (idx < 0) idx = 0;
    if (idx > (int)ls->layers->len) idx = ls->layers->len;
    g_ptr_array_insert(ls->layers, idx, l);
    ls->active = idx;
}

void layer_stack_remove(LayerStack *ls, int idx) {
    if (idx < 0 || idx >= (int)ls->layers->len) return;
    g_ptr_array_remove_index(ls->layers, idx);
    if (ls->layers->len == 0) ls->active = -1;
    else if (ls->active >= (int)ls->layers->len) ls->active = ls->layers->len - 1;
}

void layer_stack_move(LayerStack *ls, int from, int to) {
    if (from == to) return;
    if (from < 0 || from >= (int)ls->layers->len) return;
    if (to < 0 || to >= (int)ls->layers->len) return;

    Layer *l = g_ptr_array_index(ls->layers, from);
    g_ptr_array_remove_index(ls->layers, from);
    g_ptr_array_insert(ls->layers, to, l);
    ls->active = to;
}

Layer *layer_stack_get(LayerStack *ls, int idx) {
    if (!ls || idx < 0 || idx >= (int)ls->layers->len) return NULL;
    return g_ptr_array_index(ls->layers, idx);
}

Layer *layer_stack_active(LayerStack *ls) {
    return layer_stack_get(ls, ls->active);
}

void layer_stack_set_active(LayerStack *ls, int idx) {
    if (idx >= 0 && idx < (int)ls->layers->len)
        ls->active = idx;
}

GdkPixbuf *layer_stack_composite(LayerStack *ls) {
    if (!ls || ls->layers->len == 0) return NULL;

    int w = ls->width;
    int h = ls->height;

    GdkPixbuf *out = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, w, h);
    gdk_pixbuf_fill(out, 0x00000000);

    int ostride = gdk_pixbuf_get_rowstride(out);
    guchar *odata = gdk_pixbuf_get_pixels(out);

    for (guint i = 0; i < ls->layers->len; i++) {
        Layer *l = g_ptr_array_index(ls->layers, i);
        if (!l->visible || !l->pixbuf) continue;

        if (gdk_pixbuf_get_width(l->pixbuf) != w ||
            gdk_pixbuf_get_height(l->pixbuf) != h) continue;

        int n = gdk_pixbuf_get_n_channels(l->pixbuf);
        int sstride = gdk_pixbuf_get_rowstride(l->pixbuf);
        guchar *sdata = gdk_pixbuf_get_pixels(l->pixbuf);

        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                guchar *sp = sdata + y * sstride + x * n;
                guchar *dp = odata + y * ostride + x * 4;

                double top_a = (n == 4) ? (sp[3] / 255.0) : 1.0;
                top_a *= l->opacity;
                if (l->mask) {
                    guchar m = l->mask[y * w + x];
                    top_a *= (m / 255.0);
                }
                if (top_a <= 0.0) continue;

                guchar base_r = dp[0], base_g = dp[1], base_b = dp[2];
                guchar base_a = dp[3];

                guchar blend_r = blend_channel(l->blend, base_r, sp[0]);
                guchar blend_g = blend_channel(l->blend, base_g, sp[1]);
                guchar blend_b = blend_channel(l->blend, base_b, sp[2]);

                dp[0] = (guchar)(blend_r * top_a + base_r * (1.0 - top_a) + 0.5);
                dp[1] = (guchar)(blend_g * top_a + base_g * (1.0 - top_a) + 0.5);
                dp[2] = (guchar)(blend_b * top_a + base_b * (1.0 - top_a) + 0.5);
                dp[3] = (guchar)((top_a * 255.0) + (base_a * (1.0 - top_a)) + 0.5);
            }
        }
    }
    return out;
}

/* ================================================================== */
/* SHARED UI HELPERS                                                  */
/* ================================================================== */

typedef struct {
    GdkPixbuf  *current;
    GdkPixbuf  *first_original;
    GPtrArray  *undo_stack;
    SelectionMask *sel;
    LayerStack *stack;
    char       *path;
    double      zoom;

    int         shape;
    int         mode;
    int         tolerance;
    gboolean    dragging;
    int         start_x, start_y;
    int         cur_x, cur_y;
    GArray     *lasso_xs;
    GArray     *lasso_ys;

    GtkWidget  *stack_widget;
    GtkWidget  *picture;
    GtkWidget  *draw_area;
    GtkWidget  *overlay;
    GtkWidget  *root;
    GtkWidget  *undo_btn;
    GtkWidget  *sel_status;
} SelState;

static SelState *get_sel_state(GtkWidget *v) {
    return g_object_get_data(G_OBJECT(v), "sel-state");
}

static void push_sel_undo(SelState *st) {
    if (!st->current) return;
    g_ptr_array_add(st->undo_stack, gdk_pixbuf_copy(st->current));
    gtk_widget_set_sensitive(st->undo_btn, TRUE);
}

static void on_sel_undo(GtkButton *b, gpointer d) {
    (void)b;
    SelState *st = get_sel_state(d);
    if (st->undo_stack->len == 0) return;
    GdkPixbuf *prev = g_ptr_array_index(st->undo_stack,
                                          st->undo_stack->len - 1);
    g_ptr_array_remove_index(st->undo_stack, st->undo_stack->len - 1);
    g_clear_object(&st->current);
    st->current = prev;
    GdkTexture *t = gdk_texture_new_for_pixbuf(st->current);
    gtk_picture_set_paintable(GTK_PICTURE(st->picture), GDK_PAINTABLE(t));
    g_object_unref(t);
    gtk_widget_set_sensitive(st->undo_btn, st->undo_stack->len > 0);
    gtk_widget_queue_draw(st->draw_area);
}

static void clear_sel_undo(SelState *st) {
    g_ptr_array_set_size(st->undo_stack, 0);
    gtk_widget_set_sensitive(st->undo_btn, FALSE);
}

static gboolean sel_screen_to_image(SelState *st, double sx, double sy,
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

static void sel_preview_draw(GtkDrawingArea *a, cairo_t *cr,
                               int w, int h, gpointer d) {
    (void)a;
    SelState *st = d;
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

    if (st->sel && st->sel->active) {
        // Draw marching ants outline (simplified logic here)
    }

    if (st->dragging) {
        cairo_set_source_rgba(cr, 1, 1, 1, 0.9);
        cairo_set_line_width(cr, 1.5 / sx);
        cairo_set_dash(cr, (double[]){6, 4}, 2, 0);

        if (st->shape == 0) {
            double x0 = MIN(st->start_x, st->cur_x);
            double y0 = MIN(st->start_y, st->cur_y);
            double ww = abs(st->cur_x - st->start_x);
            double hh = abs(st->cur_y - st->start_y);
            cairo_rectangle(cr, x0, y0, ww, hh);
        } else if (st->shape == 1) {
            double cx = (st->start_x + st->cur_x) / 2.0;
            double cy = (st->start_y + st->cur_y) / 2.0;
            double rx = abs(st->cur_x - st->start_x) / 2.0;
            double ry = abs(st->cur_y - st->start_y) / 2.0;
            cairo_save(cr);
            cairo_translate(cr, cx, cy);
            cairo_scale(cr, rx, ry);
            cairo_arc(cr, 0, 0, 1, 0, 2 * G_PI);
            cairo_restore(cr);
        } else if (st->shape == 2 && st->lasso_xs) {
            if (st->lasso_xs->len > 0) {
                cairo_move_to(cr,
                    g_array_index(st->lasso_xs, int, 0),
                    g_array_index(st->lasso_ys, int, 0));
                for (guint i = 1; i < st->lasso_xs->len; i++)
                    cairo_line_to(cr,
                        g_array_index(st->lasso_xs, int, i),
                        g_array_index(st->lasso_ys, int, i));
            }
        }
        cairo_stroke(cr);
        cairo_set_dash(cr, NULL, 0, 0);
    }
    cairo_restore(cr);
}

static void sel_on_press(GtkGestureClick *g, int n,
                          double sx, double sy, gpointer d) {
    (void)g; (void)n;
    SelState *st = get_sel_state(d);
    double ix, iy;
    if (!sel_screen_to_image(st, sx, sy, &ix, &iy)) return;

    st->dragging = TRUE;
    st->start_x = (int)ix;
    st->start_y = (int)iy;
    st->cur_x = st->start_x;
    st->cur_y = st->start_y;

    if (st->shape == 2) {
        g_array_set_size(st->lasso_xs, 0);
        g_array_set_size(st->lasso_ys, 0);
        g_array_append_val(st->lasso_xs, st->start_x);
        g_array_append_val(st->lasso_ys, st->start_y);
    } else if (st->shape == 3) {
        if (st->sel) {
            selection_flood_from(st->sel, st->current,
                                  st->start_x, st->start_y, st->tolerance);
            char buf[64];
            snprintf(buf, sizeof buf, "Wand: %s",
                     st->sel->active ? "active" : "off");
            if (st->sel_status) gtk_label_set_text(
                GTK_LABEL(st->sel_status), buf);
        }
        st->dragging = FALSE;
    }
    gtk_widget_queue_draw(st->draw_area);
}

static void sel_on_motion(GtkEventControllerMotion *c,
                           double sx, double sy, gpointer d) {
    (void)c;
    SelState *st = get_sel_state(d);
    if (!st->dragging) return;
    double ix, iy;
    if (!sel_screen_to_image(st, sx, sy, &ix, &iy)) return;
    st->cur_x = (int)ix;
    st->cur_y = (int)iy;

    if (st->shape == 2) {
        g_array_append_val(st->lasso_xs, st->cur_x);
        g_array_append_val(st->lasso_ys, st->cur_y);
    }
    gtk_widget_queue_draw(st->draw_area);
}

static void sel_on_release(GtkGestureClick *g, int n,
                            double sx, double sy, gpointer d) {
    (void)g; (void)n;
    SelState *st = get_sel_state(d);
    if (!st->dragging) return;
    st->dragging = FALSE;
    
    double ix, iy;
    sel_screen_to_image(st, sx, sy, &ix, &iy);
    st->cur_x = (int)ix;
    st->cur_y = (int)iy;

    if (st->shape == 0) { // Rect
        selection_fill_rect(st->sel, st->start_x, st->start_y, st->cur_x, st->cur_y);
    } else if (st->shape == 1) { // Ellipse
        selection_fill_ellipse(st->sel, st->start_x, st->start_y, st->cur_x, st->cur_y);
    } else if (st->shape == 2) { // Lasso
        g_array_append_val(st->lasso_xs, st->cur_x);
        g_array_append_val(st->lasso_ys, st->cur_y);
        selection_fill_polygon(st->sel, (const int*)st->lasso_xs->data, (const int*)st->lasso_ys->data, st->lasso_xs->len);
    }

    if (st->sel_status) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Selection: %s", st->sel->shape);
        gtk_label_set_text(GTK_LABEL(st->sel_status), buf);
    }

    gtk_widget_queue_draw(st->draw_area);
}

/* ================================================================== */
/* DUMMY TOOL FACTORIES (Will be fleshed out later if needed)         */
/* ================================================================== */

static void sel_state_free(SelState *st) {
    if (!st) return;
    g_clear_object(&st->current);
    g_clear_object(&st->first_original);
    g_free(st->path);
    if (st->undo_stack) g_ptr_array_unref(st->undo_stack);
    if (st->sel) selection_free(st->sel);
    if (st->stack) layer_stack_free(st->stack);
    if (st->lasso_xs) g_array_unref(st->lasso_xs);
    if (st->lasso_ys) g_array_unref(st->lasso_ys);
    g_free(st);
}

static GtkWidget* create_dummy_tool(const char *name) {
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *lbl = gtk_label_new(name);
    gtk_box_append(GTK_BOX(root), lbl);
    return root;
}

GtkWidget *image_selection_tools_create(void) {
    return create_dummy_tool("Selection Tools UI not fully implemented");
}
GtkWidget *image_layer_manager_create(void) {
    return create_dummy_tool("Layer Manager UI not fully implemented");
}
GtkWidget *image_layer_masks_create(void) {
    return create_dummy_tool("Layer Masks UI not fully implemented");
}
GtkWidget *image_blend_modes_create(void) {
    return create_dummy_tool("Blend Modes UI not fully implemented");
}

void image_selection_tools_on_close(GtkWidget *view) {}
void image_layer_manager_on_close(GtkWidget *view) {}
void image_layer_masks_on_close(GtkWidget *view) {}
void image_blend_modes_on_close(GtkWidget *view) {}

const HelvetiaToolCommand image_selection_tools_commands[] = { {0} };
const HelvetiaToolCommand image_layer_manager_commands[] = { {0} };
const HelvetiaToolCommand image_layer_masks_commands[] = { {0} };
const HelvetiaToolCommand image_blend_modes_commands[] = { {0} };
