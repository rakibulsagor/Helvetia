#include <gtk/gtk.h>
#include <adwaita.h>
#include <math.h>
#include "../image_shared.h"
#include "image_crop.h"

/* Hit-test regions */
typedef enum {
    HIT_NONE = -1,
    HIT_MOVE,
    HIT_TL, HIT_TR, HIT_BL, HIT_BR,
    HIT_TOP, HIT_BOTTOM, HIT_LEFT, HIT_RIGHT,
} HitRegion;

#define HANDLE_HIT 16.0

typedef struct {
    GdkPixbuf *original;
    char      *path;
    int        img_w, img_h;

    double     sel_x1, sel_y1, sel_x2, sel_y2;
    gboolean   has_selection;

    double     drag_start_x, drag_start_y;
    double     orig_x1, orig_y1, orig_x2, orig_y2;
    HitRegion  active_hit;

    double     aspect_ratio;   /* 0 = free */

    GtkWidget *stack, *overlay, *picture, *draw_area;
    GtkWidget *root;
    GtkWidget *apply_btn;
} CropState;

static CropState *get_state(GtkWidget *v) {
    return g_object_get_data(G_OBJECT(v), "crop-state");
}

static void reset_selection(CropState *st) {
    st->sel_x1 = st->sel_y1 = st->sel_x2 = st->sel_y2 = 0;
    st->has_selection = FALSE;
    if (st->draw_area) gtk_widget_queue_draw(st->draw_area);
}

static void render(CropState *st) {
    if (!st->original) return;
    GdkTexture *t = gdk_texture_new_for_pixbuf(st->original);
    gtk_picture_set_paintable(GTK_PICTURE(st->picture), GDK_PAINTABLE(t));
    gtk_picture_set_content_fit(GTK_PICTURE(st->picture),
                                 GTK_CONTENT_FIT_CONTAIN);
    gtk_picture_set_can_shrink(GTK_PICTURE(st->picture), TRUE);
    g_object_unref(t);
}

/* ------------------------------------------------------------------ */
/* Selection bounds helper                                            */
/* ------------------------------------------------------------------ */

static void sel_bounds(CropState *st, double *x, double *y, double *w, double *h) {
    *x = MIN(st->sel_x1, st->sel_x2);
    *y = MIN(st->sel_y1, st->sel_y2);
    *w = fabs(st->sel_x2 - st->sel_x1);
    *h = fabs(st->sel_y2 - st->sel_y1);
}

/* ------------------------------------------------------------------ */
/* Draw                                                               */
/* ------------------------------------------------------------------ */

static void on_draw(GtkDrawingArea *area, cairo_t *cr,
                    int w, int h, gpointer data) {
    (void)area;
    CropState *st = data;
    if (!st->has_selection) return;

    double x, y, rw, rh;
    sel_bounds(st, &x, &y, &rw, &rh);

    /* Dim outside */
    cairo_set_source_rgba(cr, 0, 0, 0, 0.5);
    cairo_rectangle(cr, 0, 0, w, y); cairo_fill(cr);
    cairo_rectangle(cr, 0, y + rh, w, h - y - rh); cairo_fill(cr);
    cairo_rectangle(cr, 0, y, x, rh); cairo_fill(cr);
    cairo_rectangle(cr, x + rw, y, w - x - rw, rh); cairo_fill(cr);

    /* Selection border */
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_set_line_width(cr, 2.0);
    cairo_rectangle(cr, x, y, rw, rh);
    cairo_stroke(cr);

    /* Grid lines (rule of thirds) */
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.4);
    cairo_set_line_width(cr, 1.0);
    for (int i = 1; i < 3; i++) {
        cairo_move_to(cr, x + rw * i / 3.0, y);
        cairo_line_to(cr, x + rw * i / 3.0, y + rh);
        cairo_move_to(cr, x, y + rh * i / 3.0);
        cairo_line_to(cr, x + rw, y + rh * i / 3.0);
    }
    cairo_stroke(cr);

    /* Corner handles */
    double hs = 8.0;
    double corners[4][2] = {
        {x, y}, {x + rw, y}, {x, y + rh}, {x + rw, y + rh}
    };
    for (int i = 0; i < 4; i++) {
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_rectangle(cr, corners[i][0] - hs/2, corners[i][1] - hs/2, hs, hs);
        cairo_fill(cr);
        cairo_set_source_rgb(cr, 0.2, 0.5, 0.9);
        cairo_set_line_width(cr, 1.5);
        cairo_rectangle(cr, corners[i][0] - hs/2, corners[i][1] - hs/2, hs, hs);
        cairo_stroke(cr);
    }

    /* Edge handles (midpoints) */
    double edges[4][2] = {
        {x + rw/2, y},          /* top */
        {x + rw, y + rh/2},     /* right */
        {x + rw/2, y + rh},     /* bottom */
        {x, y + rh/2},          /* left */
    };
    for (int i = 0; i < 4; i++) {
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_arc(cr, edges[i][0], edges[i][1], 4.0, 0, 2 * G_PI);
        cairo_fill(cr);
        cairo_set_source_rgb(cr, 0.2, 0.5, 0.9);
        cairo_set_line_width(cr, 1.5);
        cairo_arc(cr, edges[i][0], edges[i][1], 4.0, 0, 2 * G_PI);
        cairo_stroke(cr);
    }
}

/* ------------------------------------------------------------------ */
/* Hit testing                                                        */
/* ------------------------------------------------------------------ */

static HitRegion hit_test(CropState *st, double x, double y) {
    if (!st->has_selection) return HIT_NONE;

    double sx, sy, sw, sh;
    sel_bounds(st, &sx, &sy, &sw, &sh);

    /* Corner handles */
    if (fabs(x - sx) < HANDLE_HIT && fabs(y - sy) < HANDLE_HIT)
        return HIT_TL;
    if (fabs(x - (sx + sw)) < HANDLE_HIT && fabs(y - sy) < HANDLE_HIT)
        return HIT_TR;
    if (fabs(x - sx) < HANDLE_HIT && fabs(y - (sy + sh)) < HANDLE_HIT)
        return HIT_BL;
    if (fabs(x - (sx + sw)) < HANDLE_HIT && fabs(y - (sy + sh)) < HANDLE_HIT)
        return HIT_BR;

    /* Edge handles */
    if (fabs(y - sy) < HANDLE_HIT && x > sx && x < sx + sw)
        return HIT_TOP;
    if (fabs(y - (sy + sh)) < HANDLE_HIT && x > sx && x < sx + sw)
        return HIT_BOTTOM;
    if (fabs(x - sx) < HANDLE_HIT && y > sy && y < sy + sh)
        return HIT_LEFT;
    if (fabs(x - (sx + sw)) < HANDLE_HIT && y > sy && y < sy + sh)
        return HIT_RIGHT;

    /* Inside → move */
    if (x > sx && x < sx + sw && y > sy && y < sy + sh)
        return HIT_MOVE;

    return HIT_NONE;
}

/* ------------------------------------------------------------------ */
/* Gesture handling                                                   */
/* ------------------------------------------------------------------ */

static void on_drag_begin(GtkGestureDrag *g, double x, double y, gpointer d) {
    (void)g;
    CropState *st = get_state(d);

    st->drag_start_x = x;
    st->drag_start_y = y;
    st->orig_x1 = st->sel_x1;
    st->orig_y1 = st->sel_y1;
    st->orig_x2 = st->sel_x2;
    st->orig_y2 = st->sel_y2;

    st->active_hit = hit_test(st, x, y);

    /* If no existing selection and no hit, start a new one */
    if (st->active_hit == HIT_NONE) {
        st->active_hit = HIT_BR;   /* treat as new-selection drag */
        st->sel_x1 = x; st->sel_y1 = y;
        st->sel_x2 = x; st->sel_y2 = y;
        st->has_selection = TRUE;
        st->orig_x1 = x; st->orig_y1 = y;
        st->orig_x2 = x; st->orig_y2 = y;
    }

    gtk_widget_queue_draw(st->draw_area);
}

static void apply_ratio_to_rect(CropState *st, double *x2, double *y2) {
    if (st->aspect_ratio <= 0) return;
    double w = *x2 - st->orig_x1;
    double h = *y2 - st->orig_y1;
    if (h == 0) return;
    w = (w < 0 ? -1 : 1) * fabs(h) * st->aspect_ratio;
    *x2 = st->orig_x1 + w;
}

static void on_drag_update(GtkGestureDrag *g, double ox, double oy, gpointer d) {
    (void)g;
    CropState *st = get_state(d);

    double nx = st->drag_start_x + ox;
    double ny = st->drag_start_y + oy;

    switch (st->active_hit) {
        case HIT_MOVE: {
            double dx = nx - st->drag_start_x;
            double dy = ny - st->drag_start_y;
            st->sel_x1 = st->orig_x1 + dx;
            st->sel_y1 = st->orig_y1 + dy;
            st->sel_x2 = st->orig_x2 + dx;
            st->sel_y2 = st->orig_y2 + dy;
            break;
        }
        case HIT_TL:
            st->sel_x1 = nx; st->sel_y1 = ny;
            break;
        case HIT_TR:
            st->sel_x2 = nx; st->sel_y1 = ny;
            apply_ratio_to_rect(st, &st->sel_x2, &st->sel_y1);
            break;
        case HIT_BL:
            st->sel_x1 = nx; st->sel_y2 = ny;
            break;
        case HIT_BR:
            st->sel_x2 = nx; st->sel_y2 = ny;
            apply_ratio_to_rect(st, &st->sel_x2, &st->sel_y2);
            break;
        case HIT_TOP:
            st->sel_y1 = ny;
            break;
        case HIT_BOTTOM:
            st->sel_y2 = ny;
            break;
        case HIT_LEFT:
            st->sel_x1 = nx;
            break;
        case HIT_RIGHT:
            st->sel_x2 = nx;
            break;
        default:
            break;
    }

    gtk_widget_queue_draw(st->draw_area);
}

static void on_drag_end(GtkGestureDrag *g, double ox, double oy, gpointer d) {
    (void)g; (void)ox; (void)oy;
    CropState *st = get_state(d);

    double sx, sy, sw, sh;
    sel_bounds(st, &sx, &sy, &sw, &sh);
    gtk_widget_set_sensitive(st->apply_btn, sw > 5 && sh > 5);
    st->active_hit = HIT_NONE;
}

/* ------------------------------------------------------------------ */
/* Ratio preset                                                       */
/* ------------------------------------------------------------------ */

static const double CROP_RATIOS[] = { 0, 1.0, 4.0/3.0, 16.0/9.0,
                                       3.0/2.0, 9.0/16.0, 2.0/3.0 };
static const char *CROP_RATIO_LABELS[] = {
    "Free", "1:1", "4:3", "16:9", "3:2", "9:16", "2:3", NULL
};

static void on_ratio_changed(GObject *dd, GParamSpec *p, gpointer d) {
    (void)p;
    CropState *st = get_state(d);
    guint i = gtk_drop_down_get_selected(GTK_DROP_DOWN(dd));
    if (i < G_N_ELEMENTS(CROP_RATIOS))
        st->aspect_ratio = CROP_RATIOS[i];
}

/* ------------------------------------------------------------------ */
/* Apply & Save                                                       */
/* ------------------------------------------------------------------ */

static void apply_crop(CropState *st) {
    if (!st->original || !st->has_selection) return;

    int sw = gtk_widget_get_width(st->draw_area);
    int sh = gtk_widget_get_height(st->draw_area);
    if (sw <= 0 || sh <= 0) return;

    double img_aspect = (double)st->img_w / st->img_h;
    double area_aspect = (double)sw / sh;

    double disp_w, disp_h, offset_x, offset_y;
    if (img_aspect > area_aspect) {
        disp_w = sw;
        disp_h = sw / img_aspect;
        offset_x = 0;
        offset_y = (sh - disp_h) / 2;
    } else {
        disp_h = sh;
        disp_w = sh * img_aspect;
        offset_x = (sw - disp_w) / 2;
        offset_y = 0;
    }

    double bx, by, bw, bh;
    sel_bounds(st, &bx, &by, &bw, &bh);

    double x1 = (bx - offset_x) / disp_w * st->img_w;
    double y1 = (by - offset_y) / disp_h * st->img_h;
    double x2 = (bx + bw - offset_x) / disp_w * st->img_w;
    double y2 = (by + bh - offset_y) / disp_h * st->img_h;

    int ix = CLAMP((int)x1, 0, st->img_w - 1);
    int iy = CLAMP((int)y1, 0, st->img_h - 1);
    int iw = CLAMP((int)(x2 - x1), 1, st->img_w - ix);
    int ih = CLAMP((int)(y2 - y1), 1, st->img_h - iy);

    GdkPixbuf *cropped = gdk_pixbuf_new(
        gdk_pixbuf_get_colorspace(st->original),
        gdk_pixbuf_get_has_alpha(st->original),
        gdk_pixbuf_get_bits_per_sample(st->original),
        iw, ih);

    gdk_pixbuf_copy_area(st->original, ix, iy, iw, ih, cropped, 0, 0);

    g_clear_object(&st->original);
    st->original = cropped;
    st->img_w = iw;
    st->img_h = ih;

    reset_selection(st);
    render(st);
    gtk_widget_set_sensitive(st->apply_btn, FALSE);
}

static void on_apply(GtkButton *b, gpointer d) { (void)b; apply_crop(get_state(d)); }

static void on_save(GtkButton *b, gpointer d) {
    (void)b;
    CropState *st = get_state(d);
    if (!st->original) return;
    char *base = st->path ? g_path_get_basename(st->path) : g_strdup("crop.png");
    char *name = g_strdup_printf("cropped_%s", base);
    image_save_pixbuf_dialog(st->root, st->original, name);
    g_free(name);
    g_free(base);
}

static void on_clear(GtkButton *b, gpointer d) {
    (void)b;
    reset_selection(get_state(d));
}

static void on_drop(const char *path, gpointer d) {
    CropState *st = get_state(d);
    GError *e = NULL;
    GdkPixbuf *pb = gdk_pixbuf_new_from_file(path, &e);
    if (!pb) { image_show_error(st->root, e->message); g_error_free(e); return; }

    g_clear_object(&st->original);
    g_free(st->path);
    st->original = pb;
    st->path = g_strdup(path);
    st->img_w = gdk_pixbuf_get_width(pb);
    st->img_h = gdk_pixbuf_get_height(pb);

    reset_selection(st);
    render(st);
    gtk_stack_set_visible_child_name(GTK_STACK(st->stack), "editor");
}

void image_crop_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "crop-state", NULL);
}

static void cmd_apply(GtkWidget *v) { apply_crop(get_state(v)); }

const HelvetiaToolCommand image_crop_commands[] = {
    { .id = "apply", .name = "Apply Crop",
      .icon_name = "object-select-symbolic",
      .accel = "<Control>Return", .tooltip = "Crop the image",
      .activate = cmd_apply },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

GtkWidget *image_crop_create(void) {
    CropState *st = g_new0(CropState, 1);
    st->active_hit = HIT_NONE;

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
    GtkWidget *editor = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(toolbar, 12);
    gtk_widget_set_margin_end(toolbar, 12);
    gtk_widget_set_margin_top(toolbar, 8);
    gtk_widget_set_margin_bottom(toolbar, 8);

    GtkWidget *hint = gtk_label_new(
        "Drag to draw · Drag inside to move · Drag corners to resize");
    gtk_widget_add_css_class(hint, "dim-label");
    gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);
    gtk_widget_set_hexpand(hint, TRUE);

    GtkWidget *ratio_lbl = gtk_label_new("Ratio:");
    gtk_widget_add_css_class(ratio_lbl, "dim-label");

    GtkWidget *ratio_dd = gtk_drop_down_new_from_strings(CROP_RATIO_LABELS);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(ratio_dd), 0);

    GtkWidget *clear = gtk_button_new_with_label("Clear");
    gtk_widget_add_css_class(clear, "flat");

    GtkWidget *apply = gtk_button_new_with_label("Apply Crop");
    gtk_widget_add_css_class(apply, "suggested-action");
    gtk_widget_set_sensitive(apply, FALSE);
    st->apply_btn = apply;

    GtkWidget *save = gtk_button_new_with_label("Save As…");
    gtk_widget_add_css_class(save, "flat");

    gtk_box_append(GTK_BOX(toolbar), hint);
    gtk_box_append(GTK_BOX(toolbar), ratio_lbl);
    gtk_box_append(GTK_BOX(toolbar), ratio_dd);
    gtk_box_append(GTK_BOX(toolbar), clear);
    gtk_box_append(GTK_BOX(toolbar), apply);
    gtk_box_append(GTK_BOX(toolbar), save);

    /* Overlay */
    GtkWidget *picture = gtk_picture_new();
    st->picture = picture;

    GtkWidget *draw = gtk_drawing_area_new();
    gtk_widget_set_hexpand(draw, TRUE);
    gtk_widget_set_vexpand(draw, TRUE);
    st->draw_area = draw;
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(draw), on_draw, st, NULL);

    GtkWidget *overlay = gtk_overlay_new();
    gtk_overlay_set_child(GTK_OVERLAY(overlay), picture);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), draw);
    gtk_widget_set_vexpand(overlay, TRUE);
    st->overlay = overlay;

    /* Gestures */
    GtkGesture *drag = gtk_gesture_drag_new();
    g_signal_connect(drag, "drag-begin",  G_CALLBACK(on_drag_begin),  root);
    g_signal_connect(drag, "drag-update", G_CALLBACK(on_drag_update), root);
    g_signal_connect(drag, "drag-end",    G_CALLBACK(on_drag_end),    root);
    gtk_widget_add_controller(draw, GTK_EVENT_CONTROLLER(drag));

    gtk_box_append(GTK_BOX(editor), toolbar);
    gtk_box_append(GTK_BOX(editor),
                   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(editor), overlay);
    gtk_stack_add_named(GTK_STACK(stack), editor, "editor");

    gtk_stack_set_visible_child_name(GTK_STACK(stack), "drop");
    gtk_box_append(GTK_BOX(root), stack);

    g_object_set_data_full(G_OBJECT(root), "crop-state", st, g_free);

    g_signal_connect(clear,   "clicked", G_CALLBACK(on_clear), root);
    g_signal_connect(apply,   "clicked", G_CALLBACK(on_apply), root);
    g_signal_connect(save,    "clicked", G_CALLBACK(on_save),  root);
    g_signal_connect(ratio_dd, "notify::selected",
                     G_CALLBACK(on_ratio_changed), root);

    return root;
}
