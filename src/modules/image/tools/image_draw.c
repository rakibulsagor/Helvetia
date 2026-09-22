#include <gtk/gtk.h>
#include <adwaita.h>
#include <cairo.h>
#include <pango/pangocairo.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "../image_shared.h"
#include "image_draw.h"

/* ================================================================== */
/* SHARED STATE                                                       */
/* ================================================================== */

typedef enum {
    TOOL_BRUSH,
    TOOL_ERASER,
    TOOL_FILL,
    TOOL_GRADIENT,
    TOOL_TEXT,
    TOOL_SHAPE,
    TOOL_ARROW,
} DrawTool;

typedef struct {
    GdkPixbuf *current;        /* mutable image */
    GdkPixbuf *first_original; /* for reset */
    GPtrArray *undo_stack;
    char      *path;

    /* Tool selector */
    DrawTool   tool;

    /* Brush / Eraser / Arrow */
    double     color_r, color_g, color_b;
    double     opacity;        /* 0..1 */
    double     size;           /* 1..200 px */
    int        eraser_mode;    /* 0 = white, 1 = transparent */

    /* Fill */
    int        fill_tolerance; /* 0..255 */

    /* Gradient */
    int        grad_type;      /* 0 = linear, 1 = radial */
    double     grad_r2, grad_g2, grad_b2;   /* second color */

    /* Text */
    char      *text_content;
    int        font_size;      /* 8..200 */
    int        font_style;     /* 0 = normal, 1 = bold, 2 = italic */
    gboolean   text_placed;      /* NEW: text is on canvas but not committed */
    double     text_x, text_y;   /* NEW: current position */
    gboolean   text_dragging;    /* NEW */
    double     text_drag_ox, text_drag_oy;   /* NEW: drag offset */

    /* Shape */
    int        shape_type;     /* 0 = rect, 1 = ellipse, 2 = line */
    gboolean   shape_filled;

    /* Interaction */
    gboolean   dragging;
    double     start_x, start_y;
    double     last_x, last_y;
    double     cur_x, cur_y;

    /* Widgets */
    double     zoom;
    GtkWidget *stack, *picture, *draw_area, *overlay, *root;
    GtkWidget *undo_btn;
} DrawState;

static DrawState *get_state(GtkWidget *v) {
    return g_object_get_data(G_OBJECT(v), "draw-state");
}

/* ================================================================== */
/* UNDO                                                               */
/* ================================================================== */

static void push_undo(DrawState *st) {
    if (!st->current) return;
    GdkPixbuf *copy = gdk_pixbuf_copy(st->current);
    g_ptr_array_add(st->undo_stack, copy);
    gtk_widget_set_sensitive(st->undo_btn, TRUE);
}

static void on_undo(GtkButton *b, gpointer d) {
    (void)b;
    DrawState *st = get_state(d);
    if (st->undo_stack->len == 0) return;

    GdkPixbuf *prev = g_ptr_array_index(st->undo_stack,
                                          st->undo_stack->len - 1);
    g_ptr_array_remove_index(st->undo_stack, st->undo_stack->len - 1);

    g_clear_object(&st->current);
    st->current = prev;   /* ownership transferred */
    gtk_picture_set_paintable(GTK_PICTURE(st->picture), GDK_PAINTABLE(gdk_texture_new_for_pixbuf(st->current)));
    gtk_widget_set_sensitive(st->undo_btn, st->undo_stack->len > 0);
    gtk_widget_queue_draw(st->draw_area);
    st->zoom = 1.0;
}

static void clear_undo(DrawState *st) {
    g_ptr_array_set_size(st->undo_stack, 0);
    gtk_widget_set_sensitive(st->undo_btn, FALSE);
}

/* ================================================================== */
/* COORDINATE MAPPING                                                 */
/* ================================================================== */

static gboolean screen_to_image(DrawState *st, double sx, double sy,
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
        dw = aw; dh = aw / img_aspect;
        ox = 0; oy = (ah - dh) / 2;
    } else {
        dh = ah; dw = ah * img_aspect;
        ox = (aw - dw) / 2; oy = 0;
    }

    *ix = (sx - ox) / dw * iw;
    *iy = (sy - oy) / dh * ih;
    return TRUE;
}

/* ================================================================== */
/* CAIRO CONTEXT OVER PIXBUF                                          */
/* ================================================================== */

static cairo_surface_t *pixbuf_surface(GdkPixbuf *pb) {
    if (!gdk_pixbuf_get_has_alpha(pb)) return NULL;
    if (gdk_pixbuf_get_n_channels(pb) != 4) return NULL;
    return cairo_image_surface_create_for_data(
        gdk_pixbuf_get_pixels(pb),
        CAIRO_FORMAT_ARGB32,
        gdk_pixbuf_get_width(pb),
        gdk_pixbuf_get_height(pb),
        gdk_pixbuf_get_rowstride(pb));
}

static void ensure_rgba(GdkPixbuf **pb) {
    if (!gdk_pixbuf_get_has_alpha(*pb)) {
        GdkPixbuf *a = gdk_pixbuf_add_alpha(*pb, FALSE, 0, 0, 0);
        g_object_unref(*pb);
        *pb = a;
    }
}

/* ================================================================== */
/* LIVE PREVIEW DRAWING                                               */
/* ================================================================== */

static void preview_draw(GtkDrawingArea *area, cairo_t *cr,
                          int w, int h, gpointer data) {
    (void)area;
    DrawState *st = data;
    if (!st->dragging || !st->current) return;

    int aw = w, ah = h;
    int iw = gdk_pixbuf_get_width(st->current);
    int ih = gdk_pixbuf_get_height(st->current);
    double img_aspect = (double)iw / ih;
    double area_aspect = (double)aw / ah;
    double dw, dh, ox, oy;
    if (img_aspect > area_aspect) {
        dw = aw; dh = aw / img_aspect; ox = 0; oy = (ah - dh) / 2;
    } else {
        dh = ah; dw = ah * img_aspect; ox = (aw - dw) / 2; oy = 0;
    }
    double sx = dw / iw;
    double sy = dh / ih;

    cairo_save(cr);
    cairo_translate(cr, ox, oy);
    cairo_scale(cr, sx, sy);

    cairo_set_source_rgba(cr, st->color_r, st->color_g,
                            st->color_b, st->opacity);
    cairo_set_line_width(cr, st->size);

    if (st->tool == TOOL_SHAPE) {
        if (st->shape_type == 0) {
            double x = MIN(st->start_x, st->cur_x);
            double y = MIN(st->start_y, st->cur_y);
            double sw = fabs(st->cur_x - st->start_x);
            double sh = fabs(st->cur_y - st->start_y);
            cairo_rectangle(cr, x, y, sw, sh);
        } else if (st->shape_type == 1) {
            double cx = (st->start_x + st->cur_x) / 2;
            double cy = (st->start_y + st->cur_y) / 2;
            double rx = fabs(st->cur_x - st->start_x) / 2;
            double ry = fabs(st->cur_y - st->start_y) / 2;
            cairo_save(cr);
            cairo_translate(cr, cx, cy);
            cairo_scale(cr, rx, ry);
            cairo_arc(cr, 0, 0, 1, 0, 2 * G_PI);
            cairo_restore(cr);
        } else {
            cairo_move_to(cr, st->start_x, st->start_y);
            cairo_line_to(cr, st->cur_x, st->cur_y);
        }
        if (st->shape_filled) cairo_fill(cr);
        else cairo_stroke(cr);
    } else if (st->tool == TOOL_ARROW) {
        cairo_move_to(cr, st->start_x, st->start_y);
        cairo_line_to(cr, st->cur_x, st->cur_y);
        cairo_stroke(cr);
        /* Arrowhead */
        double dx = st->cur_x - st->start_x;
        double dy = st->cur_y - st->start_y;
        double len = sqrt(dx*dx + dy*dy);
        if (len > 0.01) {
            double ux = dx / len, uy = dy / len;
            double ah = st->size * 4;
            double aw = st->size * 2;
            double px = st->cur_x - ux * ah;
            double py = st->cur_y - uy * ah;
            cairo_move_to(cr, st->cur_x, st->cur_y);
            cairo_line_to(cr, px - uy * aw, py + ux * aw);
            cairo_line_to(cr, px + uy * aw, py - ux * aw);
            cairo_close_path(cr);
            cairo_fill(cr);
        }
    } else if (st->tool == TOOL_GRADIENT) {
        cairo_pattern_t *pat;
        if (st->grad_type == 0) {
            pat = cairo_pattern_create_linear(st->start_x, st->start_y,
                                                st->cur_x, st->cur_y);
        } else {
            double r = sqrt(pow(st->cur_x - st->start_x, 2) +
                             pow(st->cur_y - st->start_y, 2));
            pat = cairo_pattern_create_radial(st->start_x, st->start_y, 0,
                                                st->start_x, st->start_y, r);
        }
        cairo_pattern_add_color_stop_rgba(pat, 0,
            st->color_r, st->color_g, st->color_b, st->opacity);
        cairo_pattern_add_color_stop_rgba(pat, 1,
            st->grad_r2, st->grad_g2, st->grad_b2, st->opacity);
        cairo_set_source(cr, pat);
        cairo_paint(cr);
        cairo_pattern_destroy(pat);
    }

    if (st->tool == TOOL_TEXT && st->text_placed && st->text_content) {
        cairo_set_source_rgba(cr, st->color_r, st->color_g,
                                st->color_b, st->opacity);
        PangoLayout *layout = pango_cairo_create_layout(cr);
        PangoFontDescription *desc = pango_font_description_new();
        pango_font_description_set_size(desc, st->font_size * PANGO_SCALE);
        if (st->font_style == 1)
            pango_font_description_set_weight(desc, PANGO_WEIGHT_BOLD);
        else if (st->font_style == 2)
            pango_font_description_set_style(desc, PANGO_STYLE_ITALIC);
        pango_layout_set_font_description(layout, desc);
        pango_layout_set_text(layout, st->text_content, -1);
        pango_font_description_free(desc);
        cairo_move_to(cr, st->text_x, st->text_y);
        pango_cairo_show_layout(cr, layout);
        g_object_unref(layout);

        /* Dashed border shows it's still movable */
        PangoRectangle ink;
        pango_layout_get_pixel_extents(layout, &ink, NULL);
        cairo_set_dash(cr, (double[]){4, 4}, 2, 0);
        cairo_set_line_width(cr, 1.0 / (st->opacity > 0 ? 1 : 1));
        cairo_set_source_rgba(cr, 1, 1, 1, 0.6);
        cairo_rectangle(cr, st->text_x + ink.x - 2, st->text_y + ink.y - 2,
                        ink.width + 4, ink.height + 4);
        cairo_stroke(cr);
        cairo_set_dash(cr, NULL, 0, 0);
    }

    cairo_restore(cr);
}

/* ================================================================== */
/* COMMIT TO PIXBUF                                                   */
/* ================================================================== */

static void commit_shape(DrawState *st) {
    cairo_surface_t *surf = pixbuf_surface(st->current);
    if (!surf) return;
    cairo_t *cr = cairo_create(surf);

    cairo_set_source_rgba(cr, st->color_r, st->color_g,
                            st->color_b, st->opacity);
    cairo_set_line_width(cr, st->size);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

    if (st->shape_type == 0) {
        double x = MIN(st->start_x, st->cur_x);
        double y = MIN(st->start_y, st->cur_y);
        double sw = fabs(st->cur_x - st->start_x);
        double sh = fabs(st->cur_y - st->start_y);
        cairo_rectangle(cr, x, y, sw, sh);
    } else if (st->shape_type == 1) {
        double cx = (st->start_x + st->cur_x) / 2;
        double cy = (st->start_y + st->cur_y) / 2;
        double rx = fabs(st->cur_x - st->start_x) / 2;
        double ry = fabs(st->cur_y - st->start_y) / 2;
        cairo_save(cr);
        cairo_translate(cr, cx, cy);
        cairo_scale(cr, rx, ry);
        cairo_arc(cr, 0, 0, 1, 0, 2 * G_PI);
        cairo_restore(cr);
    } else {
        cairo_move_to(cr, st->start_x, st->start_y);
        cairo_line_to(cr, st->cur_x, st->cur_y);
    }

    if (st->shape_filled && st->shape_type != 2) cairo_fill(cr);
    else cairo_stroke(cr);

    cairo_destroy(cr);
    cairo_surface_destroy(surf);
}

static void commit_arrow(DrawState *st) {
    cairo_surface_t *surf = pixbuf_surface(st->current);
    if (!surf) return;
    cairo_t *cr = cairo_create(surf);

    cairo_set_source_rgba(cr, st->color_r, st->color_g,
                            st->color_b, st->opacity);
    cairo_set_line_width(cr, st->size);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

    cairo_move_to(cr, st->start_x, st->start_y);
    cairo_line_to(cr, st->cur_x, st->cur_y);
    cairo_stroke(cr);

    double dx = st->cur_x - st->start_x;
    double dy = st->cur_y - st->start_y;
    double len = sqrt(dx*dx + dy*dy);
    if (len > 0.01) {
        double ux = dx / len, uy = dy / len;
        double ah = st->size * 4;
        double aw = st->size * 2;
        double px = st->cur_x - ux * ah;
        double py = st->cur_y - uy * ah;
        cairo_move_to(cr, st->cur_x, st->cur_y);
        cairo_line_to(cr, px - uy * aw, py + ux * aw);
        cairo_line_to(cr, px + uy * aw, py - ux * aw);
        cairo_close_path(cr);
        cairo_fill(cr);
    }
    cairo_destroy(cr);
    cairo_surface_destroy(surf);
}

static void commit_gradient(DrawState *st) {
    cairo_surface_t *surf = pixbuf_surface(st->current);
    if (!surf) return;
    cairo_t *cr = cairo_create(surf);

    cairo_pattern_t *pat;
    if (st->grad_type == 0) {
        pat = cairo_pattern_create_linear(st->start_x, st->start_y,
                                            st->cur_x, st->cur_y);
    } else {
        double r = sqrt(pow(st->cur_x - st->start_x, 2) +
                         pow(st->cur_y - st->start_y, 2));
        pat = cairo_pattern_create_radial(st->start_x, st->start_y, 0,
                                            st->start_x, st->start_y, r);
    }
    cairo_pattern_add_color_stop_rgba(pat, 0,
        st->color_r, st->color_g, st->color_b, st->opacity);
    cairo_pattern_add_color_stop_rgba(pat, 1,
        st->grad_r2, st->grad_g2, st->grad_b2, st->opacity);
    cairo_set_source(cr, pat);
    cairo_paint(cr);
    cairo_pattern_destroy(pat);

    cairo_destroy(cr);
    cairo_surface_destroy(surf);
}

static void place_text(DrawState *st, double x, double y) {
    st->text_x = x;
    st->text_y = y;
    st->text_placed = TRUE;
    gtk_widget_queue_draw(st->draw_area);
    st->zoom = 1.0;
}

static void commit_current_text(DrawState *st) {
    if (!st->text_placed || !st->text_content || !*st->text_content) return;
    push_undo(st);
    /* Same rendering logic as commit_text_at, using st->text_x/y */
    cairo_surface_t *surf = pixbuf_surface(st->current);
    if (!surf) return;
    cairo_t *cr = cairo_create(surf);
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *desc = pango_font_description_new();
    pango_font_description_set_size(desc, st->font_size * PANGO_SCALE);
    if (st->font_style == 1)
        pango_font_description_set_weight(desc, PANGO_WEIGHT_BOLD);
    else if (st->font_style == 2)
        pango_font_description_set_style(desc, PANGO_STYLE_ITALIC);
    pango_layout_set_font_description(layout, desc);
    pango_layout_set_text(layout, st->text_content, -1);
    pango_font_description_free(desc);
    cairo_set_source_rgba(cr, st->color_r, st->color_g,
                            st->color_b, st->opacity);
    cairo_move_to(cr, st->text_x, st->text_y);
    pango_cairo_show_layout(cr, layout);
    g_object_unref(layout);
    cairo_destroy(cr);
    cairo_surface_destroy(surf);

    st->text_placed = FALSE;
    gtk_picture_set_paintable(GTK_PICTURE(st->picture), GDK_PAINTABLE(gdk_texture_new_for_pixbuf(st->current)));
    gtk_widget_queue_draw(st->draw_area);
    st->zoom = 1.0;
}

/* ================================================================== */
/* BRUSH / ERASER STROKE                                              */
/* ================================================================== */

static void brush_dot(DrawState *st, double x, double y) {
    cairo_surface_t *surf = pixbuf_surface(st->current);
    if (!surf) return;
    cairo_t *cr = cairo_create(surf);

    if (st->tool == TOOL_ERASER && st->eraser_mode == 1) {
        cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    } else if (st->tool == TOOL_ERASER) {
        cairo_set_source_rgba(cr, 1, 1, 1, 1);
    } else {
        cairo_set_source_rgba(cr, st->color_r, st->color_g,
                                st->color_b, st->opacity);
    }

    cairo_set_line_width(cr, st->size);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_move_to(cr, st->last_x, st->last_y);
    cairo_line_to(cr, x, y);
    cairo_stroke(cr);

    cairo_destroy(cr);
    cairo_surface_destroy(surf);
}

static void brush_start(DrawState *st, double x, double y) {
    if (st->tool == TOOL_ERASER && st->eraser_mode == 1) {
        cairo_surface_t *surf = pixbuf_surface(st->current);
        cairo_t *cr = cairo_create(surf);
        cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
        cairo_arc(cr, x, y, st->size / 2.0, 0, 2 * G_PI);
        cairo_fill(cr);
        cairo_destroy(cr);
        cairo_surface_destroy(surf);
    } else {
        cairo_surface_t *surf = pixbuf_surface(st->current);
        cairo_t *cr = cairo_create(surf);
        if (st->tool == TOOL_ERASER)
            cairo_set_source_rgba(cr, 1, 1, 1, 1);
        else
            cairo_set_source_rgba(cr, st->color_r, st->color_g,
                                    st->color_b, st->opacity);
        cairo_arc(cr, x, y, st->size / 2.0, 0, 2 * G_PI);
        cairo_fill(cr);
        cairo_destroy(cr);
        cairo_surface_destroy(surf);
    }
}

/* ================================================================== */
/* FLOOD FILL                                                         */
/* ================================================================== */

static void flood_fill(DrawState *st, int px, int py) {
    int w = gdk_pixbuf_get_width(st->current);
    int h = gdk_pixbuf_get_height(st->current);
    int n = gdk_pixbuf_get_n_channels(st->current);
    int stride = gdk_pixbuf_get_rowstride(st->current);
    guchar *data = gdk_pixbuf_get_pixels(st->current);

    if (px < 0 || px >= w || py < 0 || py >= h) return;

    guchar *target = data + py * stride + px * n;
    guchar tr = target[0], tg = target[1], tb = target[2];
    guchar ta = (n == 4) ? target[3] : 255;

    /* New color */
    guchar nr = (guchar)(st->color_r * 255 + 0.5);
    guchar ng = (guchar)(st->color_g * 255 + 0.5);
    guchar nb = (guchar)(st->color_b * 255 + 0.5);
    guchar na = (guchar)(st->opacity * 255 + 0.5);

    /* Same color already? Skip. */
    if (abs(nr - tr) + abs(ng - tg) + abs(nb - tb) < 4) return;

    int tol = st->fill_tolerance;
    int tol2 = tol * tol * 3;

    /* Stack-based scanline flood fill */
    GQueue *queue = g_queue_new();
    /* Visited bitmap */
    guchar *visited = g_new0(guchar, w * h);

    int start_idx = py * w + px;
    g_queue_push_tail(queue, GINT_TO_POINTER(start_idx));

    while (!g_queue_is_empty(queue)) {
        int idx = GPOINTER_TO_INT(g_queue_pop_head(queue));
        if (visited[idx]) continue;

        int y = idx / w;
        int x = idx % w;
        guchar *p = data + y * stride + x * n;

        int dr = (int)p[0] - tr;
        int dg = (int)p[1] - tg;
        int db = (int)p[2] - tb;
        int da = (n == 4) ? (int)p[3] - ta : 0;
        if (dr*dr + dg*dg + db*db + da*da > tol2) continue;

        visited[idx] = 1;
        p[0] = nr;
        p[1] = ng;
        p[2] = nb;
        if (n == 4) p[3] = na;

        if (x > 0)     g_queue_push_tail(queue, GINT_TO_POINTER(idx - 1));
        if (x < w - 1) g_queue_push_tail(queue, GINT_TO_POINTER(idx + 1));
        if (y > 0)     g_queue_push_tail(queue, GINT_TO_POINTER(idx - w));
        if (y < h - 1) g_queue_push_tail(queue, GINT_TO_POINTER(idx + w));
    }

    g_queue_free(queue);
    g_free(visited);
}

/* ================================================================== */
/* GESTURES                                                           */
/* ================================================================== */

static void on_press(GtkGestureClick *g, int n_press,
                      double sx, double sy, gpointer d) {
    (void)g; (void)n_press;
    DrawState *st = get_state(d);

    double ix, iy;
    if (!screen_to_image(st, sx, sy, &ix, &iy)) return;

    switch (st->tool) {
        case TOOL_BRUSH:
        case TOOL_ERASER:
            push_undo(st);
            st->dragging = TRUE;
            st->last_x = ix;
            st->last_y = iy;
            brush_start(st, ix, iy);
            gtk_picture_set_paintable(GTK_PICTURE(st->picture), GDK_PAINTABLE(gdk_texture_new_for_pixbuf(st->current)));
            break;

        case TOOL_FILL:
            push_undo(st);
            flood_fill(st, (int)ix, (int)iy);
            gtk_picture_set_paintable(GTK_PICTURE(st->picture), GDK_PAINTABLE(gdk_texture_new_for_pixbuf(st->current)));
            break;

        case TOOL_GRADIENT:
        case TOOL_SHAPE:
        case TOOL_ARROW:
            st->dragging = TRUE;
            st->start_x = ix; st->start_y = iy;
            st->cur_x = ix; st->cur_y = iy;
            break;

        case TOOL_TEXT:
            if (!st->text_placed) {
                place_text(st, ix, iy);
            } else {
                st->text_dragging = TRUE;
                st->text_drag_ox = ix - st->text_x;
                st->text_drag_oy = iy - st->text_y;
            }
            break;
    }
}

static void on_motion(GtkEventControllerMotion *c, double sx, double sy,
                       gpointer d) {
    (void)c;
    DrawState *st = get_state(d);
    if (!st->dragging) return;

    double ix, iy;
    if (!screen_to_image(st, sx, sy, &ix, &iy)) return;

    if (st->tool == TOOL_BRUSH || st->tool == TOOL_ERASER) {
        brush_dot(st, ix, iy);
        st->last_x = ix;
        st->last_y = iy;
        gtk_picture_set_paintable(GTK_PICTURE(st->picture), GDK_PAINTABLE(gdk_texture_new_for_pixbuf(st->current)));
    } else if (st->tool == TOOL_GRADIENT ||
               st->tool == TOOL_SHAPE ||
               st->tool == TOOL_ARROW) {
        st->cur_x = ix;
        st->cur_y = iy;
        gtk_widget_queue_draw(st->draw_area);
    st->zoom = 1.0;
    } else if (st->tool == TOOL_TEXT && st->text_dragging) {
        st->text_x = ix - st->text_drag_ox;
        st->text_y = iy - st->text_drag_oy;
        gtk_widget_queue_draw(st->draw_area);
    st->zoom = 1.0;
        return;
    }
}

static void on_release(GtkGestureClick *g, int n_press,
                        double sx, double sy, gpointer d) {
    (void)g; (void)n_press; (void)sx; (void)sy;
    DrawState *st = get_state(d);
    if (!st->dragging) return;

    push_undo(st);
    switch (st->tool) {
        case TOOL_GRADIENT: commit_gradient(st); break;
        case TOOL_SHAPE:    commit_shape(st);    break;
        case TOOL_ARROW:    commit_arrow(st);    break;
        default: break;
    }
    st->dragging = FALSE;
    if (st->tool == TOOL_TEXT) {
        st->text_dragging = FALSE;
    }
    gtk_picture_set_paintable(GTK_PICTURE(st->picture), GDK_PAINTABLE(gdk_texture_new_for_pixbuf(st->current)));
    gtk_widget_queue_draw(st->draw_area);
    st->zoom = 1.0;
}

/* ================================================================== */
/* COMMON SHELL                                                       */
/* ================================================================== */

static GtkWidget *make_slider(const char *label, double min, double max,
                                double step, double initial,
                                GtkWidget **out_lbl, GCallback cb,
                                gpointer user_data) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *lbl = gtk_label_new(label);
    gtk_widget_add_css_class(lbl, "dim-label");
    gtk_widget_set_size_request(lbl, 90, -1);
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

/* Color picker */
typedef struct {
    DrawState *st;
    double    *r, *g, *b;
} ColorCtx;

static void on_color_picked(GObject *btn, GParamSpec *p, gpointer d) {
    (void)p;
    ColorCtx *ctx = d;
    const GdkRGBA *color = gtk_color_dialog_button_get_rgba(GTK_COLOR_DIALOG_BUTTON(btn));
    if (color) {
        *ctx->r = color->red;
        *ctx->g = color->green;
        *ctx->b = color->blue;
    }
}

static GtkWidget *make_color_button(const char *label,
                                      double *r, double *g, double *b,
                                      DrawState *st) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *lbl = gtk_label_new(label);
    gtk_widget_add_css_class(lbl, "dim-label");
    gtk_widget_set_size_request(lbl, 90, -1);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
    gtk_widget_set_hexpand(lbl, TRUE);

    GdkRGBA initial = {*r, *g, *b, 1.0};
    GtkColorDialog *dialog = gtk_color_dialog_new();
    GtkWidget *btn = gtk_color_dialog_button_new(dialog);
    gtk_color_dialog_button_set_rgba(GTK_COLOR_DIALOG_BUTTON(btn), &initial);

    ColorCtx *ctx = g_new0(ColorCtx, 1);
    ctx->st = st;
    ctx->r = r; ctx->g = g; ctx->b = b;
    g_signal_connect_data(btn, "notify::rgba",
        G_CALLBACK(on_color_picked), ctx,
        (GClosureNotify)g_free, 0);

    gtk_box_append(GTK_BOX(row), lbl);
    gtk_box_append(GTK_BOX(row), btn);
    return row;
}

/* Save / Load */
static void on_save_common(DrawState *st, const char *prefix) {
    if (!st->current) return;
    char *base = st->path ? g_path_get_basename(st->path) : g_strdup("image.png");
    char *name = g_strdup_printf("%s_%s", prefix, base);
    image_save_pixbuf_dialog(st->root, st->current, name);
    g_free(name);
    g_free(base);
}

static void on_drop_common(DrawState *st, const char *path) {
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
    DrawState *st = get_state(d);
    if (!st->first_original) return;
    push_undo(st);
    g_clear_object(&st->current);
    st->current = g_object_ref(st->first_original);
    gtk_picture_set_paintable(GTK_PICTURE(st->picture), GDK_PAINTABLE(gdk_texture_new_for_pixbuf(st->current)));
}

static void draw_state_free(DrawState *st) {
    if (!st) return;
    g_clear_object(&st->current);
    g_clear_object(&st->first_original);
    if (st->undo_stack) g_ptr_array_unref(st->undo_stack);
    g_free(st->text_content);
    g_free(st->path);
    g_free(st);
}

/* Build the shared editor shell. Returns the stack. */
static GtkWidget *build_shell(DrawState *st, GtkWidget **out_opts,
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

    /* Canvas: picture + overlay drawing area */
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

    /* Gestures */
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
/* UI callbacks for shared sliders                                    */
/* ================================================================== */

static void on_size(GtkRange *r, gpointer d) {
    DrawState *st = get_state(d);
    st->size = gtk_range_get_value(r);
}
static void on_opacity(GtkRange *r, gpointer d) {
    DrawState *st = get_state(d);
    st->opacity = gtk_range_get_value(r) / 100.0;
}

/* ================================================================== */
/* TOOL 43 — BRUSH                                                    */
/* ================================================================== */

static void br_save(GtkButton *b, gpointer d) { (void)b; on_save_common(get_state(d), "brushed"); }
static void br_drop(const char *p, gpointer d) { on_drop_common(get_state(d), p); }
static void br_reset(GtkButton *b, gpointer d) { on_reset_common(b, d); }

void image_brush_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "draw-state", NULL);
}
static void cmd_br_reset(GtkWidget *v) { on_reset_common(NULL, v); }

const HelvetiaToolCommand image_brush_commands[] = {
    { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic",
      .accel = NULL, .tooltip = "Reset", .activate = cmd_br_reset },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

GtkWidget *image_brush_create(void) {
    DrawState *st = g_new0(DrawState, 1);
    st->undo_stack = g_ptr_array_new();
    st->zoom = 1.0;
    st->tool = TOOL_BRUSH;
    st->color_r = 0.0; st->color_g = 0.0; st->color_b = 0.0;
    st->opacity = 1.0;
    st->size = 10;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;

    GtkWidget *opts;
    GtkWidget *stack = build_shell(st, &opts, "Image file",
                                    br_drop, G_CALLBACK(br_save),
                                    G_CALLBACK(br_reset), root);

    GtkWidget *l1, *l2;
    gtk_box_append(GTK_BOX(opts),
        make_color_button("Color", &st->color_r, &st->color_g, &st->color_b, st));
    gtk_box_append(GTK_BOX(opts),
        make_slider("Size", 1, 200, 1, 10, &l1, G_CALLBACK(on_size), root));
    gtk_label_set_text(GTK_LABEL(l1), "10");
    gtk_box_append(GTK_BOX(opts),
        make_slider("Opacity %", 1, 100, 1, 100, &l2, G_CALLBACK(on_opacity), root));
    gtk_label_set_text(GTK_LABEL(l2), "100");

    GtkWidget *hint = gtk_label_new("Click and drag to paint. Round-cap strokes for smooth lines.");
    gtk_label_set_wrap(GTK_LABEL(hint), TRUE);
    gtk_widget_add_css_class(hint, "dim-label");
    gtk_widget_add_css_class(hint, "caption");
    gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);
    gtk_box_append(GTK_BOX(opts), hint);

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "draw-state", st,
                           (GDestroyNotify)draw_state_free);
    image_register_zoom(root, st->picture, &st->zoom);
    image_install_zoom_shortcuts(root);
    return root;
}

/* ================================================================== */
/* TOOL 44 — ERASER                                                   */
/* ================================================================== */

static void er_save(GtkButton *b, gpointer d) { (void)b; on_save_common(get_state(d), "erased"); }
static void er_drop(const char *p, gpointer d) { on_drop_common(get_state(d), p); }
static void er_reset(GtkButton *b, gpointer d) { on_reset_common(b, d); }

static void er_on_mode(GObject *dd, GParamSpec *p, gpointer d) {
    (void)p;
    DrawState *st = get_state(d);
    st->eraser_mode = gtk_drop_down_get_selected(GTK_DROP_DOWN(dd));
}

void image_eraser_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "draw-state", NULL);
}
static void cmd_er_reset(GtkWidget *v) { on_reset_common(NULL, v); }

const HelvetiaToolCommand image_eraser_commands[] = {
    { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic",
      .accel = NULL, .tooltip = "Reset", .activate = cmd_er_reset },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

GtkWidget *image_eraser_create(void) {
    DrawState *st = g_new0(DrawState, 1);
    st->undo_stack = g_ptr_array_new();
    st->zoom = 1.0;
    st->tool = TOOL_ERASER;
    st->size = 20;
    st->eraser_mode = 0;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;

    GtkWidget *opts;
    GtkWidget *stack = build_shell(st, &opts, "Image file",
                                    er_drop, G_CALLBACK(er_save),
                                    G_CALLBACK(er_reset), root);

    GtkWidget *mode_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *ml = gtk_label_new("Mode:");
    gtk_widget_add_css_class(ml, "dim-label");
    gtk_widget_set_size_request(ml, 90, -1);
    gtk_label_set_xalign(GTK_LABEL(ml), 0.0f);
    const char *modes[] = {"White", "Transparent", NULL};
    GtkWidget *mode_dd = gtk_drop_down_new_from_strings(modes);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(mode_dd), 0);
    gtk_box_append(GTK_BOX(mode_row), ml);
    gtk_box_append(GTK_BOX(mode_row), mode_dd);
    gtk_box_append(GTK_BOX(opts), mode_row);

    GtkWidget *l1;
    gtk_box_append(GTK_BOX(opts),
        make_slider("Size", 1, 200, 1, 20, &l1, G_CALLBACK(on_size), root));
    gtk_label_set_text(GTK_LABEL(l1), "20");

    g_signal_connect(mode_dd, "notify::selected",
                     G_CALLBACK(er_on_mode), root);

    GtkWidget *hint = gtk_label_new(
        "White mode paints white (good for photos). "
        "Transparent mode makes pixels transparent (good for PNGs).");
    gtk_label_set_wrap(GTK_LABEL(hint), TRUE);
    gtk_widget_add_css_class(hint, "dim-label");
    gtk_widget_add_css_class(hint, "caption");
    gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);
    gtk_box_append(GTK_BOX(opts), hint);

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "draw-state", st,
                           (GDestroyNotify)draw_state_free);
    image_register_zoom(root, st->picture, &st->zoom);
    image_install_zoom_shortcuts(root);
    return root;
}

/* ================================================================== */
/* TOOL 45 — FILL / BUCKET                                            */
/* ================================================================== */

static void fl_save(GtkButton *b, gpointer d) { (void)b; on_save_common(get_state(d), "filled"); }
static void fl_drop(const char *p, gpointer d) { on_drop_common(get_state(d), p); }
static void fl_reset(GtkButton *b, gpointer d) { on_reset_common(b, d); }

static void fl_on_tol(GtkRange *r, gpointer d) {
    DrawState *st = get_state(d);
    st->fill_tolerance = (int)gtk_range_get_value(r);
}

void image_fill_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "draw-state", NULL);
}
static void cmd_fl_reset(GtkWidget *v) { on_reset_common(NULL, v); }

const HelvetiaToolCommand image_fill_commands[] = {
    { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic",
      .accel = NULL, .tooltip = "Reset", .activate = cmd_fl_reset },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

GtkWidget *image_fill_create(void) {
    DrawState *st = g_new0(DrawState, 1);
    st->undo_stack = g_ptr_array_new();
    st->zoom = 1.0;
    st->tool = TOOL_FILL;
    st->color_r = 0.2; st->color_g = 0.5; st->color_b = 0.9;
    st->opacity = 1.0;
    st->fill_tolerance = 30;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;

    GtkWidget *opts;
    GtkWidget *stack = build_shell(st, &opts, "Image file",
                                    fl_drop, G_CALLBACK(fl_save),
                                    G_CALLBACK(fl_reset), root);

    GtkWidget *l1, *l2;
    gtk_box_append(GTK_BOX(opts),
        make_color_button("Fill color", &st->color_r, &st->color_g, &st->color_b, st));
    gtk_box_append(GTK_BOX(opts),
        make_slider("Tolerance", 0, 255, 1, 30, &l1, G_CALLBACK(fl_on_tol), root));
    gtk_label_set_text(GTK_LABEL(l1), "30");
    gtk_box_append(GTK_BOX(opts),
        make_slider("Opacity %", 1, 100, 1, 100, &l2, G_CALLBACK(on_opacity), root));
    gtk_label_set_text(GTK_LABEL(l2), "100");

    GtkWidget *hint = gtk_label_new(
        "Click a region to flood-fill with the chosen color. "
        "Higher tolerance fills more similar pixels.");
    gtk_label_set_wrap(GTK_LABEL(hint), TRUE);
    gtk_widget_add_css_class(hint, "dim-label");
    gtk_widget_add_css_class(hint, "caption");
    gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);
    gtk_box_append(GTK_BOX(opts), hint);

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "draw-state", st,
                           (GDestroyNotify)draw_state_free);
    image_register_zoom(root, st->picture, &st->zoom);
    image_install_zoom_shortcuts(root);
    return root;
}

/* ================================================================== */
/* TOOL 46 — GRADIENT                                                 */
/* ================================================================== */

static void gr_save(GtkButton *b, gpointer d) { (void)b; on_save_common(get_state(d), "gradient"); }
static void gr_drop(const char *p, gpointer d) { on_drop_common(get_state(d), p); }
static void gr_reset(GtkButton *b, gpointer d) { on_reset_common(b, d); }

static void gr_on_type(GObject *dd, GParamSpec *p, gpointer d) {
    (void)p;
    DrawState *st = get_state(d);
    st->grad_type = gtk_drop_down_get_selected(GTK_DROP_DOWN(dd));
}

void image_gradient_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "draw-state", NULL);
}
static void cmd_gr_reset(GtkWidget *v) { on_reset_common(NULL, v); }

const HelvetiaToolCommand image_gradient_commands[] = {
    { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic",
      .accel = NULL, .tooltip = "Reset", .activate = cmd_gr_reset },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

GtkWidget *image_gradient_create(void) {
    DrawState *st = g_new0(DrawState, 1);
    st->undo_stack = g_ptr_array_new();
    st->zoom = 1.0;
    st->tool = TOOL_GRADIENT;
    st->color_r = 0.0; st->color_g = 0.0; st->color_b = 0.0;
    st->grad_r2 = 1.0; st->grad_g2 = 1.0; st->grad_b2 = 1.0;
    st->opacity = 1.0;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;

    GtkWidget *opts;
    GtkWidget *stack = build_shell(st, &opts, "Image file",
                                    gr_drop, G_CALLBACK(gr_save),
                                    G_CALLBACK(gr_reset), root);

    GtkWidget *type_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *tl = gtk_label_new("Type:");
    gtk_widget_add_css_class(tl, "dim-label");
    gtk_widget_set_size_request(tl, 90, -1);
    gtk_label_set_xalign(GTK_LABEL(tl), 0.0f);
    const char *types[] = {"Linear", "Radial", NULL};
    GtkWidget *type_dd = gtk_drop_down_new_from_strings(types);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(type_dd), 0);
    gtk_box_append(GTK_BOX(type_row), tl);
    gtk_box_append(GTK_BOX(type_row), type_dd);
    gtk_box_append(GTK_BOX(opts), type_row);

    gtk_box_append(GTK_BOX(opts),
        make_color_button("Color 1", &st->color_r, &st->color_g, &st->color_b, st));
    gtk_box_append(GTK_BOX(opts),
        make_color_button("Color 2", &st->grad_r2, &st->grad_g2, &st->grad_b2, st));

    GtkWidget *l;
    gtk_box_append(GTK_BOX(opts),
        make_slider("Opacity %", 1, 100, 1, 100, &l, G_CALLBACK(on_opacity), root));
    gtk_label_set_text(GTK_LABEL(l), "100");

    g_signal_connect(type_dd, "notify::selected",
                     G_CALLBACK(gr_on_type), root);

    GtkWidget *hint = gtk_label_new(
        "Click and drag to draw a gradient. "
        "Release to commit. Linear goes from start to end point; "
        "Radial expands from start point.");
    gtk_label_set_wrap(GTK_LABEL(hint), TRUE);
    gtk_widget_add_css_class(hint, "dim-label");
    gtk_widget_add_css_class(hint, "caption");
    gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);
    gtk_box_append(GTK_BOX(opts), hint);

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "draw-state", st,
                           (GDestroyNotify)draw_state_free);
    image_register_zoom(root, st->picture, &st->zoom);
    image_install_zoom_shortcuts(root);
    return root;
}

/* ================================================================== */
/* TOOL 47 — TEXT                                                     */
/* ================================================================== */

static void tx_save(GtkButton *b, gpointer d) { (void)b; on_save_common(get_state(d), "text"); }
static void tx_drop(const char *p, gpointer d) { on_drop_common(get_state(d), p); }
static void tx_reset(GtkButton *b, gpointer d) { on_reset_common(b, d); }

static void tx_on_text(GtkEditable *e, gpointer d) {
    DrawState *st = get_state(d);
    g_free(st->text_content);
    st->text_content = g_strdup(gtk_editable_get_text(e));
}
static void tx_on_font_size(GtkRange *r, gpointer d) {
    DrawState *st = get_state(d);
    st->font_size = (int)gtk_range_get_value(r);
}
static void tx_on_style(GObject *dd, GParamSpec *p, gpointer d) {
    (void)p;
    DrawState *st = get_state(d);
    st->font_style = gtk_drop_down_get_selected(GTK_DROP_DOWN(dd));
}

void image_text_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "draw-state", NULL);
}
static void cmd_tx_reset(GtkWidget *v) { on_reset_common(NULL, v); }

const HelvetiaToolCommand image_text_commands[] = {
    { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic",
      .accel = NULL, .tooltip = "Reset", .activate = cmd_tx_reset },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

GtkWidget *image_text_create(void) {
    DrawState *st = g_new0(DrawState, 1);
    st->undo_stack = g_ptr_array_new();
    st->zoom = 1.0;
    st->tool = TOOL_TEXT;
    st->color_r = 0.0; st->color_g = 0.0; st->color_b = 0.0;
    st->opacity = 1.0;
    st->font_size = 32;
    st->font_style = 0;
    st->text_content = g_strdup("Hello");

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;

    GtkWidget *opts;
    GtkWidget *stack = build_shell(st, &opts, "Image file",
                                    tx_drop, G_CALLBACK(tx_save),
                                    G_CALLBACK(tx_reset), root);

    GtkWidget *text_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *tl = gtk_label_new("Text:");
    gtk_widget_add_css_class(tl, "dim-label");
    gtk_widget_set_size_request(tl, 90, -1);
    gtk_label_set_xalign(GTK_LABEL(tl), 0.0f);
    GtkWidget *entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(entry), "Hello");
    gtk_widget_set_hexpand(entry, TRUE);
    gtk_box_append(GTK_BOX(text_row), tl);
    gtk_box_append(GTK_BOX(text_row), entry);
    gtk_box_append(GTK_BOX(opts), text_row);

    GtkWidget *style_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *sl = gtk_label_new("Style:");
    gtk_widget_add_css_class(sl, "dim-label");
    gtk_widget_set_size_request(sl, 90, -1);
    gtk_label_set_xalign(GTK_LABEL(sl), 0.0f);
    const char *styles[] = {"Normal", "Bold", "Italic", NULL};
    GtkWidget *style_dd = gtk_drop_down_new_from_strings(styles);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(style_dd), 0);
    gtk_box_append(GTK_BOX(style_row), sl);
    gtk_box_append(GTK_BOX(style_row), style_dd);
    gtk_box_append(GTK_BOX(opts), style_row);

    GtkWidget *commit_btn = gtk_button_new_with_label("Apply Text to Image");
    gtk_widget_add_css_class(commit_btn, "suggested-action");
    g_signal_connect_swapped(commit_btn, "clicked",
        G_CALLBACK(commit_current_text), st);
    gtk_box_append(GTK_BOX(opts), commit_btn);

    gtk_box_append(GTK_BOX(opts),
        make_color_button("Color", &st->color_r, &st->color_g, &st->color_b, st));

    GtkWidget *l1, *l2;
    gtk_box_append(GTK_BOX(opts),
        make_slider("Font size", 8, 200, 1, 32, &l1, G_CALLBACK(tx_on_font_size), root));
    gtk_label_set_text(GTK_LABEL(l1), "32");
    gtk_box_append(GTK_BOX(opts),
        make_slider("Opacity %", 1, 100, 1, 100, &l2, G_CALLBACK(on_opacity), root));
    gtk_label_set_text(GTK_LABEL(l2), "100");

    g_signal_connect(entry, "changed", G_CALLBACK(tx_on_text), root);
    g_signal_connect(style_dd, "notify::selected", G_CALLBACK(tx_on_style), root);

    GtkWidget *hint = gtk_label_new(
        "Click on the image to place the text. "
        "The click position becomes the text baseline.");
    gtk_label_set_wrap(GTK_LABEL(hint), TRUE);
    gtk_widget_add_css_class(hint, "dim-label");
    gtk_widget_add_css_class(hint, "caption");
    gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);
    gtk_box_append(GTK_BOX(opts), hint);

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "draw-state", st,
                           (GDestroyNotify)draw_state_free);
    image_register_zoom(root, st->picture, &st->zoom);
    image_install_zoom_shortcuts(root);
    return root;
}

/* ================================================================== */
/* TOOL 48 — SHAPE                                                    */
/* ================================================================== */

static void sp_save(GtkButton *b, gpointer d) { (void)b; on_save_common(get_state(d), "shape"); }
static void sp_drop(const char *p, gpointer d) { on_drop_common(get_state(d), p); }
static void sp_reset(GtkButton *b, gpointer d) { on_reset_common(b, d); }

static void sp_on_type(GObject *dd, GParamSpec *p, gpointer d) {
    (void)p;
    DrawState *st = get_state(d);
    st->shape_type = gtk_drop_down_get_selected(GTK_DROP_DOWN(dd));
}
static void sp_on_filled(GObject *sw, GParamSpec *p, gpointer d) {
    (void)p;
    DrawState *st = get_state(d);
    st->shape_filled = gtk_switch_get_active(GTK_SWITCH(sw));
}

void image_shape_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "draw-state", NULL);
}
static void cmd_sp_reset(GtkWidget *v) { on_reset_common(NULL, v); }

const HelvetiaToolCommand image_shape_commands[] = {
    { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic",
      .accel = NULL, .tooltip = "Reset", .activate = cmd_sp_reset },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

GtkWidget *image_shape_create(void) {
    DrawState *st = g_new0(DrawState, 1);
    st->undo_stack = g_ptr_array_new();
    st->zoom = 1.0;
    st->tool = TOOL_SHAPE;
    st->color_r = 0.9; st->color_g = 0.1; st->color_b = 0.1;
    st->opacity = 1.0;
    st->size = 4;
    st->shape_type = 0;
    st->shape_filled = FALSE;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;

    GtkWidget *opts;
    GtkWidget *stack = build_shell(st, &opts, "Image file",
                                    sp_drop, G_CALLBACK(sp_save),
                                    G_CALLBACK(sp_reset), root);

    GtkWidget *type_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *tl = gtk_label_new("Shape:");
    gtk_widget_add_css_class(tl, "dim-label");
    gtk_widget_set_size_request(tl, 90, -1);
    gtk_label_set_xalign(GTK_LABEL(tl), 0.0f);
    const char *types[] = {"Rectangle", "Ellipse", "Line", NULL};
    GtkWidget *type_dd = gtk_drop_down_new_from_strings(types);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(type_dd), 0);
    gtk_box_append(GTK_BOX(type_row), tl);
    gtk_box_append(GTK_BOX(type_row), type_dd);
    gtk_box_append(GTK_BOX(opts), type_row);

    gtk_box_append(GTK_BOX(opts),
        make_color_button("Color", &st->color_r, &st->color_g, &st->color_b, st));

    GtkWidget *l1, *l2;
    gtk_box_append(GTK_BOX(opts),
        make_slider("Line width", 1, 40, 1, 4, &l1, G_CALLBACK(on_size), root));
    gtk_label_set_text(GTK_LABEL(l1), "4");
    gtk_box_append(GTK_BOX(opts),
        make_slider("Opacity %", 1, 100, 1, 100, &l2, G_CALLBACK(on_opacity), root));
    gtk_label_set_text(GTK_LABEL(l2), "100");

    /* Filled toggle */
    GtkWidget *fill_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *fl = gtk_label_new("Filled");
    gtk_widget_set_size_request(fl, 90, -1);
    gtk_label_set_xalign(GTK_LABEL(fl), 0.0f);
    gtk_widget_set_hexpand(fl, TRUE);
    GtkWidget *fill_sw = gtk_switch_new();
    gtk_widget_set_valign(fill_sw, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(fill_row), fl);
    gtk_box_append(GTK_BOX(fill_row), fill_sw);
    gtk_box_append(GTK_BOX(opts), fill_row);

    g_signal_connect(type_dd, "notify::selected",
                     G_CALLBACK(sp_on_type), root);
    g_signal_connect(fill_sw, "notify::active",
                     G_CALLBACK(sp_on_filled), root);

    GtkWidget *hint = gtk_label_new(
        "Click and drag to draw a shape. "
        "Filled works for rectangle and ellipse; line is always stroked.");
    gtk_label_set_wrap(GTK_LABEL(hint), TRUE);
    gtk_widget_add_css_class(hint, "dim-label");
    gtk_widget_add_css_class(hint, "caption");
    gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);
    gtk_box_append(GTK_BOX(opts), hint);

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "draw-state", st,
                           (GDestroyNotify)draw_state_free);
    image_register_zoom(root, st->picture, &st->zoom);
    image_install_zoom_shortcuts(root);
    return root;
}

/* ================================================================== */
/* TOOL 49 — ARROW                                                    */
/* ================================================================== */

static void ar_save(GtkButton *b, gpointer d) { (void)b; on_save_common(get_state(d), "arrow"); }
static void ar_drop(const char *p, gpointer d) { on_drop_common(get_state(d), p); }
static void ar_reset(GtkButton *b, gpointer d) { on_reset_common(b, d); }

void image_arrow_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "draw-state", NULL);
}
static void cmd_ar_reset(GtkWidget *v) { on_reset_common(NULL, v); }

const HelvetiaToolCommand image_arrow_commands[] = {
    { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic",
      .accel = NULL, .tooltip = "Reset", .activate = cmd_ar_reset },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

GtkWidget *image_arrow_create(void) {
    DrawState *st = g_new0(DrawState, 1);
    st->undo_stack = g_ptr_array_new();
    st->zoom = 1.0;
    st->tool = TOOL_ARROW;
    st->color_r = 0.9; st->color_g = 0.1; st->color_b = 0.1;
    st->opacity = 1.0;
    st->size = 4;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;

    GtkWidget *opts;
    GtkWidget *stack = build_shell(st, &opts, "Image file",
                                    ar_drop, G_CALLBACK(ar_save),
                                    G_CALLBACK(ar_reset), root);

    gtk_box_append(GTK_BOX(opts),
        make_color_button("Color", &st->color_r, &st->color_g, &st->color_b, st));

    GtkWidget *l1, *l2;
    gtk_box_append(GTK_BOX(opts),
        make_slider("Line width", 1, 40, 1, 4, &l1, G_CALLBACK(on_size), root));
    gtk_label_set_text(GTK_LABEL(l1), "4");
    gtk_box_append(GTK_BOX(opts),
        make_slider("Opacity %", 1, 100, 1, 100, &l2, G_CALLBACK(on_opacity), root));
    gtk_label_set_text(GTK_LABEL(l2), "100");

    GtkWidget *hint = gtk_label_new(
        "Click and drag to draw an arrow. "
        "The arrowhead size scales with the line width.");
    gtk_label_set_wrap(GTK_LABEL(hint), TRUE);
    gtk_widget_add_css_class(hint, "dim-label");
    gtk_widget_add_css_class(hint, "caption");
    gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);
    gtk_box_append(GTK_BOX(opts), hint);

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "draw-state", st,
                           (GDestroyNotify)draw_state_free);
    image_register_zoom(root, st->picture, &st->zoom);
    image_install_zoom_shortcuts(root);
    return root;
}
