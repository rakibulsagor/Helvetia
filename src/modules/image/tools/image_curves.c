#include <gtk/gtk.h>
#include <adwaita.h>
#include <math.h>
#include <string.h>
#include "../image_shared.h"
#include "image_curves.h"

#define MAX_POINTS 16
#define HIT_RADIUS 12.0

typedef struct {
    double x, y;   /* both 0..1 */
} CurvePoint;

typedef struct {
    GdkPixbuf *original;
    GdkPixbuf *preview;
    GdkPixbuf *first_original;
    GPtrArray *undo_stack;   /* CurvePoint snapshots */
    char      *path;

    CurvePoint points[MAX_POINTS];
    int        n_points;

    /* Per-channel enable */
    int        channel;      /* 0=RGB, 1=R, 2=G, 3=B */

    /* Four separate curves, one per channel */
    CurvePoint channels[4][MAX_POINTS];
    int        channels_n[4];

    int        drag_idx;

    GtkWidget *stack, *picture, *curve_area, *root;
    GtkWidget *undo_btn;
    GtkWidget *channel_dd;
    GtkWidget *hist_area;
    guint      histogram[256];
    guint      hist_max;
} CurvesState;

static CurvesState *get_state(GtkWidget *v) {
    return g_object_get_data(G_OBJECT(v), "curves-state");
}

/* ------------------------------------------------------------------ */
/* Default curve = identity (2 endpoints)                             */
/* ------------------------------------------------------------------ */

static void default_curve(CurvePoint *pts, int *n) {
    pts[0] = (CurvePoint){0.0, 0.0};
    pts[1] = (CurvePoint){1.0, 1.0};
    *n = 2;
}

static void reset_all_curves(CurvesState *st) {
    for (int c = 0; c < 4; c++)
        default_curve(st->channels[c], &st->channels_n[c]);
    memcpy(st->points, st->channels[st->channel],
           sizeof(CurvePoint) * MAX_POINTS);
    st->n_points = st->channels_n[st->channel];
    gtk_widget_queue_draw(st->curve_area);
}

/* ------------------------------------------------------------------ */
/* LUT building                                                       */
/* ------------------------------------------------------------------ */

/* Catmull-Rom spline through control points → 256-entry LUT */
static void build_lut(CurvePoint *pts, int n, guchar lut[256]) {
    if (n < 2) {
        for (int i = 0; i < 256; i++) lut[i] = (guchar)i;
        return;
    }
    for (int i = 0; i < 256; i++) {
        double x = i / 255.0;
        double y = 0.0;

        if (x <= pts[0].x) y = pts[0].y;
        else if (x >= pts[n-1].x) y = pts[n-1].y;
        else {
            /* Find segment */
            int seg = 0;
            for (int j = 0; j < n - 1; j++) {
                if (x >= pts[j].x && x <= pts[j+1].x) { seg = j; break; }
            }
            double x0 = pts[seg].x,   y0 = pts[seg].y;
            double x1 = pts[seg+1].x, y1 = pts[seg+1].y;
            double t = (x - x0) / (x1 - x0 + 1e-9);
            /* Smoothstep for gentle interpolation */
            double s = t * t * (3.0 - 2.0 * t);
            y = y0 + (y1 - y0) * s;
        }

        if (y < 0) y = 0;
        if (y > 1) y = 1;
        lut[i] = (guchar)(y * 255.0 + 0.5);
    }
}

/* ------------------------------------------------------------------ */
/* Preview                                                            */
/* ------------------------------------------------------------------ */

static void update_preview(CurvesState *st) {
    GdkPixbuf *src = st->preview ? st->preview : st->original;
    if (!src) return;
    GdkTexture *t = gdk_texture_new_for_pixbuf(src);
    gtk_picture_set_paintable(GTK_PICTURE(st->picture), GDK_PAINTABLE(t));
    gtk_picture_set_content_fit(GTK_PICTURE(st->picture),
                                 GTK_CONTENT_FIT_CONTAIN);
    gtk_picture_set_can_shrink(GTK_PICTURE(st->picture), TRUE);
    g_object_unref(t);
}

static gboolean is_identity(CurvesState *st) {
    for (int c = 0; c < 4; c++) {
        if (st->channels_n[c] != 2) return FALSE;
        if (fabs(st->channels[c][0].x) > 0.001) return FALSE;
        if (fabs(st->channels[c][0].y) > 0.001) return FALSE;
        if (fabs(st->channels[c][1].x - 1.0) > 0.001) return FALSE;
        if (fabs(st->channels[c][1].y - 1.0) > 0.001) return FALSE;
    }
    return TRUE;
}

static GdkPixbuf *apply_curves(CurvesState *st) {
    GdkPixbuf *src = st->original;
    int w = gdk_pixbuf_get_width(src);
    int h = gdk_pixbuf_get_height(src);
    int n = gdk_pixbuf_get_n_channels(src);
    int stride = gdk_pixbuf_get_rowstride(src);
    guchar *px = gdk_pixbuf_get_pixels(src);

    GdkPixbuf *out = gdk_pixbuf_copy(src);
    int ostride = gdk_pixbuf_get_rowstride(out);
    guchar *opx = gdk_pixbuf_get_pixels(out);

    /* Build 4 LUTs */
    guchar lut_rgb[256], lut_r[256], lut_g[256], lut_b[256];
    build_lut(st->channels[0], st->channels_n[0], lut_rgb);
    build_lut(st->channels[1], st->channels_n[1], lut_r);
    build_lut(st->channels[2], st->channels_n[2], lut_g);
    build_lut(st->channels[3], st->channels_n[3], lut_b);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            guchar *sp = px + y * stride + x * n;
            guchar *dp = opx + y * ostride + x * n;

            /* Apply RGB curve first, then per-channel */
            guchar r = lut_rgb[sp[0]];
            guchar g = lut_rgb[sp[1]];
            guchar b = lut_rgb[sp[2]];

            dp[0] = lut_r[r];
            dp[1] = lut_g[g];
            dp[2] = lut_b[b];
            if (n == 4) dp[3] = sp[3];
        }
    }
    return out;
}

static void regenerate(CurvesState *st) {
    if (!st->original) return;
    if (is_identity(st)) {
        g_clear_object(&st->preview);
        update_preview(st);
        return;
    }
    g_clear_object(&st->preview);
    st->preview = apply_curves(st);
    update_preview(st);
}

/* ------------------------------------------------------------------ */
/* Histogram                                                          */
/* ------------------------------------------------------------------ */

static void compute_histogram(CurvesState *st) {
    memset(st->histogram, 0, sizeof st->histogram);
    st->hist_max = 0;
    if (!st->original) return;

    int w = gdk_pixbuf_get_width(st->original);
    int h = gdk_pixbuf_get_height(st->original);
    int n = gdk_pixbuf_get_n_channels(st->original);
    int stride = gdk_pixbuf_get_rowstride(st->original);
    guchar *px = gdk_pixbuf_get_pixels(st->original);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            guchar *p = px + y * stride + x * n;
            double lum = 0.2126 * p[0] + 0.7152 * p[1] + 0.0722 * p[2];
            int bin = CLAMP((int)lum, 0, 255);
            st->histogram[bin]++;
        }
    }
    for (int i = 2; i < 254; i++)
        if (st->histogram[i] > st->hist_max) st->hist_max = st->histogram[i];
    if (st->hist_max == 0) st->hist_max = 1;
}

/* ------------------------------------------------------------------ */
/* Curve editor drawing                                               */
/* ------------------------------------------------------------------ */

static void on_curve_draw(GtkDrawingArea *area, cairo_t *cr,
                          int w, int h, gpointer data) {
    (void)area;
    CurvesState *st = data;

    /* Background */
    cairo_set_source_rgb(cr, 0.08, 0.08, 0.08);
    cairo_paint(cr);

    /* Grid */
    cairo_set_source_rgba(cr, 1, 1, 1, 0.1);
    cairo_set_line_width(cr, 1);
    for (int i = 1; i < 4; i++) {
        cairo_move_to(cr, w * i / 4.0, 0);
        cairo_line_to(cr, w * i / 4.0, h);
        cairo_move_to(cr, 0, h * i / 4.0);
        cairo_line_to(cr, w, h * i / 4.0);
    }
    cairo_stroke(cr);

    /* Diagonal reference */
    cairo_set_source_rgba(cr, 1, 1, 1, 0.15);
    cairo_move_to(cr, 0, h);
    cairo_line_to(cr, w, 0);
    cairo_stroke(cr);

    /* Draw curve */
    cairo_set_source_rgb(cr, 0.20, 0.60, 1.0);
    cairo_set_line_width(cr, 2.0);

    guchar lut[256];
    build_lut(st->points, st->n_points, lut);

    cairo_move_to(cr, 0, h - lut[0] * h / 255.0);
    for (int i = 1; i < 256; i++) {
        cairo_line_to(cr, i * w / 255.0, h - lut[i] * h / 255.0);
    }
    cairo_stroke(cr);

    /* Draw control points */
    for (int i = 0; i < st->n_points; i++) {
        double px = st->points[i].x * w;
        double py = h - st->points[i].y * h;

        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_arc(cr, px, py, 5, 0, 2 * G_PI);
        cairo_fill(cr);

        cairo_set_source_rgb(cr, 0.20, 0.60, 1.0);
        cairo_set_line_width(cr, 2);
        cairo_arc(cr, px, py, 5, 0, 2 * G_PI);
        cairo_stroke(cr);
    }

    /* Border */
    cairo_set_source_rgba(cr, 1, 1, 1, 0.15);
    cairo_set_line_width(cr, 1);
    cairo_rectangle(cr, 0.5, 0.5, w - 1, h - 1);
    cairo_stroke(cr);
}

static int curve_hit_test(CurvesState *st, double cx, double cy,
                          int w, int h) {
    for (int i = 0; i < st->n_points; i++) {
        double px = st->points[i].x * w;
        double py = h - st->points[i].y * h;
        double dx = cx - px, dy = cy - py;
        if (dx*dx + dy*dy < HIT_RADIUS * HIT_RADIUS)
            return i;
    }
    return -1;
}

/* Sort points by x after moving */
static void sort_points(CurvesState *st) {
    for (int i = 1; i < st->n_points; i++) {
        CurvePoint p = st->points[i];
        int j = i - 1;
        while (j >= 0 && st->points[j].x > p.x) {
            st->points[j+1] = st->points[j];
            j--;
        }
        st->points[j+1] = p;
    }
}

/* ---- Undo ---- */

typedef struct {
    CurvePoint channels[4][MAX_POINTS];
    int        channels_n[4];
    int        channel;
} CurvesSnapshot;

static void push_snapshot(CurvesState *st) {
    CurvesSnapshot *s = g_new0(CurvesSnapshot, 1);
    memcpy(s->channels, st->channels, sizeof s->channels);
    memcpy(s->channels_n, st->channels_n, sizeof s->channels_n);
    s->channel = st->channel;
    g_ptr_array_add(st->undo_stack, s);
    gtk_widget_set_sensitive(st->undo_btn, TRUE);
}

static void restore_snapshot(CurvesState *st, CurvesSnapshot *s) {
    memcpy(st->channels, s->channels, sizeof st->channels);
    memcpy(st->channels_n, s->channels_n, sizeof st->channels_n);
    st->channel = s->channel;
    gtk_drop_down_set_selected(GTK_DROP_DOWN(st->channel_dd), s->channel);
    memcpy(st->points, st->channels[st->channel],
           sizeof(CurvePoint) * MAX_POINTS);
    st->n_points = st->channels_n[st->channel];
    gtk_widget_queue_draw(st->curve_area);
    regenerate(st);
}

/* ---- Drag ---- */

static void on_press(GtkGestureClick *g, int n, double x, double y, gpointer d) {
    (void)g; (void)n;
    CurvesState *st = get_state(d);
    int w = gtk_widget_get_width(st->curve_area);
    int h = gtk_widget_get_height(st->curve_area);

    int idx = curve_hit_test(st, x, y, w, h);

    if (idx >= 0) {
        /* Select existing point */
        st->drag_idx = idx;
        return;
    }

    /* Add new point */
    if (st->n_points >= MAX_POINTS) return;
    double nx = CLAMP(x / w, 0.0, 1.0);
    double ny = CLAMP(1.0 - y / h, 0.0, 1.0);

    push_snapshot(st);

    st->points[st->n_points] = (CurvePoint){nx, ny};
    st->n_points++;
    sort_points(st);
    /* Find new index after sort */
    for (int i = 0; i < st->n_points; i++) {
        if (fabs(st->points[i].x - nx) < 0.001 &&
            fabs(st->points[i].y - ny) < 0.001) {
            st->drag_idx = i;
            break;
        }
    }
    gtk_widget_queue_draw(st->curve_area);
    regenerate(st);
}

static void on_motion(GtkEventControllerMotion *c, double x, double y,
                       gpointer d) {
    (void)c;
    CurvesState *st = get_state(d);
    if (st->drag_idx < 0) return;

    int w = gtk_widget_get_width(st->curve_area);
    int h = gtk_widget_get_height(st->curve_area);
    if (w <= 0 || h <= 0) return;

    double nx = CLAMP(x / w, 0.0, 1.0);
    double ny = CLAMP(1.0 - y / h, 0.0, 1.0);

    /* Endpoints locked to x=0 and x=1 */
    if (st->drag_idx == 0) nx = 0.0;
    if (st->drag_idx == st->n_points - 1) nx = 1.0;

    /* Keep x within neighbors */
    if (st->drag_idx > 0)
        nx = MAX(nx, st->points[st->drag_idx-1].x + 0.005);
    if (st->drag_idx < st->n_points - 1)
        nx = MIN(nx, st->points[st->drag_idx+1].x - 0.005);

    st->points[st->drag_idx].x = nx;
    st->points[st->drag_idx].y = ny;

    /* Save to channel */
    memcpy(st->channels[st->channel], st->points,
           sizeof(CurvePoint) * MAX_POINTS);
    st->channels_n[st->channel] = st->n_points;

    gtk_widget_queue_draw(st->curve_area);
    regenerate(st);
}

static void on_release(GtkGestureClick *g, int n, double x, double y,
                       gpointer d) {
    (void)g; (void)n; (void)x; (void)y;
    CurvesState *st = get_state(d);
    st->drag_idx = -1;
}

/* Right-click → delete a point */
static void on_right_press(GtkGestureClick *g, int n,
                            double x, double y, gpointer d) {
    (void)g; (void)n;
    CurvesState *st = get_state(d);
    int w = gtk_widget_get_width(st->curve_area);
    int h = gtk_widget_get_height(st->curve_area);

    int idx = curve_hit_test(st, x, y, w, h);
    /* Can't delete endpoints */
    if (idx <= 0 || idx >= st->n_points - 1) return;

    push_snapshot(st);

    for (int i = idx; i < st->n_points - 1; i++)
        st->points[i] = st->points[i+1];
    st->n_points--;

    memcpy(st->channels[st->channel], st->points,
           sizeof(CurvePoint) * MAX_POINTS);
    st->channels_n[st->channel] = st->n_points;

    gtk_widget_queue_draw(st->curve_area);
    regenerate(st);
}

/* ------------------------------------------------------------------ */
/* Channel dropdown                                                   */
/* ------------------------------------------------------------------ */

static void on_channel_changed(GObject *dd, GParamSpec *p, gpointer d) {
    (void)p;
    CurvesState *st = get_state(d);
    st->channel = gtk_drop_down_get_selected(GTK_DROP_DOWN(dd));
    memcpy(st->points, st->channels[st->channel],
           sizeof(CurvePoint) * MAX_POINTS);
    st->n_points = st->channels_n[st->channel];
    gtk_widget_queue_draw(st->curve_area);
}

/* ------------------------------------------------------------------ */
/* Undo / Reset                                                       */
/* ------------------------------------------------------------------ */

static void on_undo(GtkButton *b, gpointer d) {
    (void)b;
    CurvesState *st = get_state(d);
    if (st->undo_stack->len == 0) return;

    CurvesSnapshot *s = g_ptr_array_index(st->undo_stack,
                                            st->undo_stack->len - 1);
    g_ptr_array_remove_index(st->undo_stack, st->undo_stack->len - 1);
    restore_snapshot(st, s);
    g_free(s);
    gtk_widget_set_sensitive(st->undo_btn, st->undo_stack->len > 0);
}

static void on_reset(GtkButton *b, gpointer d) {
    (void)b;
    CurvesState *st = get_state(d);
    push_snapshot(st);
    reset_all_curves(st);
    regenerate(st);
}

/* ------------------------------------------------------------------ */
/* Save / Load                                                        */
/* ------------------------------------------------------------------ */

static void on_save(GtkButton *b, gpointer d) {
    (void)b;
    CurvesState *st = get_state(d);
    GdkPixbuf *src = st->preview ? st->preview : st->original;
    if (!src) return;
    char *base = st->path ? g_path_get_basename(st->path) : g_strdup("image.png");
    char *name = g_strdup_printf("curves_%s", base);
    image_save_pixbuf_dialog(st->root, src, name);
    g_free(name);
    g_free(base);
}

static void on_drop(const char *path, gpointer d) {
    CurvesState *st = get_state(d);
    GError *e = NULL;
    GdkPixbuf *pb = gdk_pixbuf_new_from_file(path, &e);
    if (!pb) { image_show_error(st->root, e->message); g_error_free(e); return; }

    g_clear_object(&st->original);
    g_clear_object(&st->preview);
    g_clear_object(&st->first_original);
    g_free(st->path);

    /* Clear undo history */
    for (guint i = 0; i < st->undo_stack->len; i++)
        g_free(g_ptr_array_index(st->undo_stack, i));
    g_ptr_array_set_size(st->undo_stack, 0);
    gtk_widget_set_sensitive(st->undo_btn, FALSE);

    st->original = pb;
    st->first_original = g_object_ref(pb);
    st->path = g_strdup(path);

    reset_all_curves(st);
    compute_histogram(st);
    update_preview(st);
    gtk_stack_set_visible_child_name(GTK_STACK(st->stack), "editor");
}

void image_curves_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "curves-state", NULL);
}

/* ------------------------------------------------------------------ */
/* State free                                                         */
/* ------------------------------------------------------------------ */

static void curves_state_free(CurvesState *st) {
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
    CurvesState *st = get_state(v);
    push_snapshot(st);
    reset_all_curves(st);
    regenerate(st);
}

const HelvetiaToolCommand image_curves_commands[] = {
    { .id = "reset", .name = "Reset",
      .icon_name = "view-refresh-symbolic",
      .accel = NULL, .tooltip = "Reset curves",
      .activate = cmd_reset },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

/* ------------------------------------------------------------------ */
/* Create                                                             */
/* ------------------------------------------------------------------ */

GtkWidget *image_curves_create(void) {
    CurvesState *st = g_new0(CurvesState, 1);
    st->drag_idx = -1;
    st->channel = 0;
    st->undo_stack = g_ptr_array_new();
    for (int c = 0; c < 4; c++) default_curve(st->channels[c], &st->channels_n[c]);
    memcpy(st->points, st->channels[0], sizeof(CurvePoint) * MAX_POINTS);
    st->n_points = st->channels_n[0];

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;

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

    /* Editor page */
    GtkWidget *editor = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

    /* Left: curve editor */
    GtkWidget *curve = gtk_drawing_area_new();
    gtk_widget_set_size_request(curve, 300, 300);
    gtk_widget_set_margin_start(curve, 16);
    gtk_widget_set_margin_end(curve, 16);
    gtk_widget_set_margin_top(curve, 16);
    gtk_widget_set_margin_bottom(curve, 16);
    st->curve_area = curve;
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(curve),
                                    on_curve_draw, st, NULL);

    GtkGesture *click = gtk_gesture_click_new();
    g_signal_connect(click, "pressed",  G_CALLBACK(on_press),  root);
    g_signal_connect(click, "released", G_CALLBACK(on_release), root);
    gtk_widget_add_controller(curve, GTK_EVENT_CONTROLLER(click));

    GtkGesture *rclick = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(rclick), 3);
    g_signal_connect(rclick, "pressed", G_CALLBACK(on_right_press), root);
    gtk_widget_add_controller(curve, GTK_EVENT_CONTROLLER(rclick));

    GtkEventController *motion = gtk_event_controller_motion_new();
    g_signal_connect(motion, "motion", G_CALLBACK(on_motion), root);
    gtk_widget_add_controller(curve, motion);

    /* Right: controls + picture */
    GtkWidget *right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(right, TRUE);

    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(bar, 12);
    gtk_widget_set_margin_end(bar, 12);
    gtk_widget_set_margin_top(bar, 8);
    gtk_widget_set_margin_bottom(bar, 8);

    GtkWidget *chan_lbl = gtk_label_new("Channel:");
    gtk_widget_add_css_class(chan_lbl, "dim-label");
    const char *chans[] = {"RGB", "Red", "Green", "Blue", NULL};
    st->channel_dd = gtk_drop_down_new_from_strings(chans);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(st->channel_dd), 0);

    st->undo_btn = image_undo_button(G_CALLBACK(on_undo), root);
    gtk_widget_set_sensitive(st->undo_btn, FALSE);
    GtkWidget *reset = image_reset_button(G_CALLBACK(on_reset), root);

    GtkWidget *sp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(sp, TRUE);

    GtkWidget *save = gtk_button_new_with_label("Save As…");
    gtk_widget_add_css_class(save, "flat");

    gtk_box_append(GTK_BOX(bar), chan_lbl);
    gtk_box_append(GTK_BOX(bar), st->channel_dd);
    gtk_box_append(GTK_BOX(bar), gtk_separator_new(GTK_ORIENTATION_VERTICAL));
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

    gtk_box_append(GTK_BOX(right), bar);
    gtk_box_append(GTK_BOX(right),
                   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(right), pic);

    gtk_box_append(GTK_BOX(editor), curve);
    gtk_box_append(GTK_BOX(editor),
                   gtk_separator_new(GTK_ORIENTATION_VERTICAL));
    gtk_box_append(GTK_BOX(editor), right);

    gtk_stack_add_named(GTK_STACK(stack), editor, "editor");
    gtk_stack_set_visible_child_name(GTK_STACK(stack), "drop");
    gtk_box_append(GTK_BOX(root), stack);

    g_object_set_data_full(G_OBJECT(root), "curves-state", st,
                           (GDestroyNotify)curves_state_free);

    g_signal_connect(st->channel_dd, "notify::selected",
                     G_CALLBACK(on_channel_changed), root);
    g_signal_connect(save, "clicked", G_CALLBACK(on_save), root);

    return root;
}
