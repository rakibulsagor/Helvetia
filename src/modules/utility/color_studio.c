#include "color_studio.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define PI 3.14159265358979323846

/* ------------------------------------------------------------------------- */
/* State                                                                     */
/* ------------------------------------------------------------------------- */
typedef struct {
    double h; /* 0..360 */
    double s; /* 0..1   */
    double v; /* 0..1   */
    double a; /* 0..1   */
    
    GtkWidget *sv_area;
    GtkWidget *hue_area;
    GtkWidget *alpha_area;
    
    GtkWidget *hex_entry;
    GtkWidget *rgb_entry;
    GtkWidget *hsl_entry;
    GtkWidget *oklch_entry;
    GtkWidget *css_entry;
    
    GtkWidget *copy_btn;
    GtkWidget *harmony_dd;
    GtkWidget *harmony_box;
    
    gboolean updating;
} ColorState;

static void update_ui_from_state(ColorState *state);

/* ------------------------------------------------------------------------- */
/* Math helpers                                                              */
/* ------------------------------------------------------------------------- */

static void hsv_to_rgb(double h, double s, double v, double *r, double *g, double *b) {
    if (s <= 0.0) { *r = v; *g = v; *b = v; return; }
    double hh = h;
    if (hh >= 360.0) hh = 0.0;
    hh /= 60.0;
    int i = (int)hh;
    double ff = hh - i;
    double p = v * (1.0 - s);
    double q = v * (1.0 - (s * ff));
    double t = v * (1.0 - (s * (1.0 - ff)));
    
    switch(i) {
        case 0: *r = v; *g = t; *b = p; break;
        case 1: *r = q; *g = v; *b = p; break;
        case 2: *r = p; *g = v; *b = t; break;
        case 3: *r = p; *g = q; *b = v; break;
        case 4: *r = t; *g = p; *b = v; break;
        case 5: default: *r = v; *g = p; *b = q; break;
    }
}

static void rgb_to_hsv(double r, double g, double b, double *h, double *s, double *v) {
    double min = r < g ? r : g; min = min < b ? min : b;
    double max = r > g ? r : g; max = max > b ? max : b;
    *v = max;
    double delta = max - min;
    if (delta < 0.00001) { *s = 0; *h = 0; return; }
    if (max > 0.0) { *s = (delta / max); } else { *s = 0.0; *h = 0.0; return; }
    
    if (r >= max) *h = (g - b) / delta;
    else if (g >= max) *h = 2.0 + (b - r) / delta;
    else *h = 4.0 + (r - g) / delta;
    
    *h *= 60.0;
    if (*h < 0.0) *h += 360.0;
}

static void rgb_to_hsl(double r, double g, double b, double *h, double *s, double *l) {
    double min = r < g ? r : g; min = min < b ? min : b;
    double max = r > g ? r : g; max = max > b ? max : b;
    *l = (max + min) / 2.0;
    if (max == min) { *h = 0; *s = 0; return; }
    double d = max - min;
    *s = *l > 0.5 ? d / (2.0 - max - min) : d / (max + min);
    if (max == r) *h = (g - b) / d + (g < b ? 6 : 0);
    else if (max == g) *h = (b - r) / d + 2;
    else *h = (r - g) / d + 4;
    *h /= 6.0;
    *h *= 360.0;
}

static void rgb_to_oklch(double r, double g, double b, double *l, double *c, double *h) {
    double hh, ss, ll;
    rgb_to_hsl(r, g, b, &hh, &ss, &ll);
    *h = hh;
    *c = ss * 0.322;
    *l = ll; 
}

/* ------------------------------------------------------------------------- */
/* Draw callbacks                                                            */
/* ------------------------------------------------------------------------- */

static void draw_sv(GtkDrawingArea *area, cairo_t *cr, int w, int h, gpointer data) {
    ColorState *state = data;
    double r, g, b;
    hsv_to_rgb(state->h, 1.0, 1.0, &r, &g, &b);
    
    cairo_set_source_rgb(cr, r, g, b);
    cairo_paint(cr);

    cairo_pattern_t *sat = cairo_pattern_create_linear(0, 0, w, 0);
    cairo_pattern_add_color_stop_rgba(sat, 0, 1, 1, 1, 1);
    cairo_pattern_add_color_stop_rgba(sat, 1, 1, 1, 1, 0);
    cairo_set_source(cr, sat);
    cairo_paint(cr);
    cairo_pattern_destroy(sat);

    cairo_pattern_t *val = cairo_pattern_create_linear(0, 0, 0, h);
    cairo_pattern_add_color_stop_rgba(val, 0, 0, 0, 0, 0);
    cairo_pattern_add_color_stop_rgba(val, 1, 0, 0, 0, 1);
    cairo_set_source(cr, val);
    cairo_paint(cr);
    cairo_pattern_destroy(val);
    
    double hx = state->s * w;
    double hy = (1.0 - state->v) * h;
    
    cairo_arc(cr, hx, hy, 8, 0, 2*PI);
    cairo_set_source_rgba(cr, 1,1,1,1);
    cairo_set_line_width(cr, 2);
    cairo_stroke_preserve(cr);
    cairo_set_source_rgba(cr, 0,0,0,0.2);
    cairo_fill(cr);
}

static void draw_hue(GtkDrawingArea *area, cairo_t *cr, int w, int h, gpointer data) {
    ColorState *state = data;
    
    cairo_pattern_t *pat = cairo_pattern_create_linear(0, 0, w, 0);
    for (int i=0; i<=6; i++) {
        double r, g, b;
        hsv_to_rgb(i * 60.0, 1.0, 1.0, &r, &g, &b);
        cairo_pattern_add_color_stop_rgba(pat, i / 6.0, r, g, b, 1.0);
    }
    
    cairo_arc(cr, h/2.0, h/2.0, h/2.0, PI/2, 3*PI/2);
    cairo_arc(cr, w - h/2.0, h/2.0, h/2.0, -PI/2, PI/2);
    cairo_close_path(cr);
    cairo_clip(cr);
    
    cairo_set_source(cr, pat);
    cairo_paint(cr);
    cairo_pattern_destroy(pat);
    
    double hx = (state->h / 360.0) * w;
    cairo_arc(cr, hx, h/2.0, h/2.0 - 2, 0, 2*PI);
    cairo_set_source_rgba(cr, 1,1,1,1);
    cairo_set_line_width(cr, 3);
    cairo_stroke_preserve(cr);
    cairo_set_source_rgba(cr, 0,0,0,0.5);
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);
}

static void draw_alpha(GtkDrawingArea *area, cairo_t *cr, int w, int h, gpointer data) {
    ColorState *state = data;
    
    cairo_set_source_rgb(cr, 0.8, 0.8, 0.8);
    cairo_paint(cr);
    cairo_set_source_rgb(cr, 1, 1, 1);
    for (int y=0; y<h; y+=8) {
        for (int x=0; x<w; x+=8) {
            if ((x/8 + y/8) % 2 == 0) {
                cairo_rectangle(cr, x, y, 8, 8);
                cairo_fill(cr);
            }
        }
    }
    
    cairo_arc(cr, h/2.0, h/2.0, h/2.0, PI/2, 3*PI/2);
    cairo_arc(cr, w - h/2.0, h/2.0, h/2.0, -PI/2, PI/2);
    cairo_close_path(cr);
    cairo_clip(cr);
    
    double r,g,b;
    hsv_to_rgb(state->h, state->s, state->v, &r, &g, &b);
    cairo_pattern_t *pat = cairo_pattern_create_linear(0, 0, w, 0);
    cairo_pattern_add_color_stop_rgba(pat, 0, r, g, b, 0);
    cairo_pattern_add_color_stop_rgba(pat, 1, r, g, b, 1);
    cairo_set_source(cr, pat);
    cairo_paint(cr);
    cairo_pattern_destroy(pat);
    
    double hx = state->a * w;
    cairo_arc(cr, hx, h/2.0, h/2.0 - 2, 0, 2*PI);
    cairo_set_source_rgba(cr, 1,1,1,1);
    cairo_set_line_width(cr, 3);
    cairo_stroke_preserve(cr);
    cairo_set_source_rgba(cr, 0,0,0,0.5);
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);
}

/* ------------------------------------------------------------------------- */
/* Gestures                                                                  */
/* ------------------------------------------------------------------------- */

static void update_sv_from_coords(ColorState *state, double x, double y) {
    int w = gtk_widget_get_width(state->sv_area);
    int h = gtk_widget_get_height(state->sv_area);
    if (w <= 0 || h <= 0) return;
    
    double s = x / w;
    double v = 1.0 - (y / h);
    if (s < 0) s = 0; if (s > 1) s = 1;
    if (v < 0) v = 0; if (v > 1) v = 1;
    
    state->s = s;
    state->v = v;
    update_ui_from_state(state);
}

static void update_hue_from_coords(ColorState *state, double x) {
    int w = gtk_widget_get_width(state->hue_area);
    if (w <= 0) return;
    double h = (x / w) * 360.0;
    if (h < 0) h = 0; if (h >= 360) h = 359.99;
    state->h = h;
    update_ui_from_state(state);
}

static void update_alpha_from_coords(ColorState *state, double x) {
    int w = gtk_widget_get_width(state->alpha_area);
    if (w <= 0) return;
    double a = x / w;
    if (a < 0) a = 0; if (a > 1) a = 1;
    state->a = a;
    update_ui_from_state(state);
}

static void on_sv_drag(GtkGestureDrag *g, double dx, double dy, gpointer data) {
    ColorState *state = data;
    double sx, sy;
    gtk_gesture_drag_get_start_point(g, &sx, &sy);
    update_sv_from_coords(state, sx + dx, sy + dy);
}

static void on_sv_pressed(GtkGestureClick *g, int n_press, double x, double y, gpointer data) {
    ColorState *state = data;
    update_sv_from_coords(state, x, y);
}

static void on_hue_drag(GtkGestureDrag *g, double dx, double dy, gpointer data) {
    ColorState *state = data;
    double sx, sy;
    gtk_gesture_drag_get_start_point(g, &sx, &sy);
    update_hue_from_coords(state, sx + dx);
}

static void on_hue_pressed(GtkGestureClick *g, int n_press, double x, double y, gpointer data) {
    ColorState *state = data;
    update_hue_from_coords(state, x);
}

static void on_alpha_drag(GtkGestureDrag *g, double dx, double dy, gpointer data) {
    ColorState *state = data;
    double sx, sy;
    gtk_gesture_drag_get_start_point(g, &sx, &sy);
    update_alpha_from_coords(state, sx + dx);
}

static void on_alpha_pressed(GtkGestureClick *g, int n_press, double x, double y, gpointer data) {
    ColorState *state = data;
    update_alpha_from_coords(state, x);
}

/* ------------------------------------------------------------------------- */
/* State sync                                                                */
/* ------------------------------------------------------------------------- */

static GtkWidget *make_swatch(double h, double s, double v) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_size_request(box, 36, 36);
    gtk_widget_add_css_class(box, "helvetia-swatch");
    
    double r, g, b;
    hsv_to_rgb(h, s, v, &r, &g, &b);
    
    char css[128];
    snprintf(css, sizeof css, ".helvetia-swatch { background-color: rgb(%d,%d,%d); border-radius: 6px; }", 
             (int)(r*255), (int)(g*255), (int)(b*255));
    GtkCssProvider *p = gtk_css_provider_new();
    gtk_css_provider_load_from_string(p, css);
    gtk_style_context_add_provider(gtk_widget_get_style_context(box), GTK_STYLE_PROVIDER(p), 600);
    g_object_unref(p);
    
    return box;
}

static void update_harmonies(ColorState *state) {
    // Clear box
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(state->harmony_box)) != NULL) {
        gtk_box_remove(GTK_BOX(state->harmony_box), child);
    }
    
    guint mode = gtk_drop_down_get_selected(GTK_DROP_DOWN(state->harmony_dd));
    // 0: Complementary, 1: Analogous, 2: Triadic
    
    gtk_box_append(GTK_BOX(state->harmony_box), make_swatch(state->h, state->s, state->v));
    
    if (mode == 0) { // Comp
        double h2 = state->h + 180.0;
        if (h2 >= 360) h2 -= 360;
        gtk_box_append(GTK_BOX(state->harmony_box), make_swatch(h2, state->s, state->v));
    } else if (mode == 1) { // Analogous
        double h2 = state->h + 30.0; if (h2 >= 360) h2 -= 360;
        double h3 = state->h - 30.0; if (h3 < 0) h3 += 360;
        gtk_box_append(GTK_BOX(state->harmony_box), make_swatch(h2, state->s, state->v));
        gtk_box_append(GTK_BOX(state->harmony_box), make_swatch(h3, state->s, state->v));
    } else if (mode == 2) { // Triadic
        double h2 = state->h + 120.0; if (h2 >= 360) h2 -= 360;
        double h3 = state->h + 240.0; if (h3 >= 360) h3 -= 360;
        gtk_box_append(GTK_BOX(state->harmony_box), make_swatch(h2, state->s, state->v));
        gtk_box_append(GTK_BOX(state->harmony_box), make_swatch(h3, state->s, state->v));
    }
}

static void update_ui_from_state(ColorState *state) {
    if (state->updating) return;
    state->updating = TRUE;
    
    double r, g, b, hl, sl, ll, ol, oc, oh;
    hsv_to_rgb(state->h, state->s, state->v, &r, &g, &b);
    rgb_to_hsl(r, g, b, &hl, &sl, &ll);
    rgb_to_oklch(r, g, b, &ol, &oc, &oh);
    
    int ri = (int)(r * 255.0);
    int gi = (int)(g * 255.0);
    int bi = (int)(b * 255.0);
    
    char hex[16], rgb[64], hsl[64], oklch[64], css[128];
    snprintf(hex, sizeof hex, "#%02X%02X%02X", ri, gi, bi);
    snprintf(rgb, sizeof rgb, "rgb(%d, %d, %d)", ri, gi, bi);
    snprintf(hsl, sizeof hsl, "hsl(%d, %d%%, %d%%)", (int)hl, (int)(sl*100), (int)(ll*100));
    snprintf(oklch, sizeof oklch, "oklch(%.2f %.2f %d)", ol, oc, (int)oh);
    if (state->a < 1.0) {
        snprintf(css, sizeof css, "color: rgba(%d, %d, %d, %.2f);", ri, gi, bi, state->a);
    } else {
        snprintf(css, sizeof css, "color: %s;", hex);
    }
    
    gtk_editable_set_text(GTK_EDITABLE(state->hex_entry), hex);
    gtk_editable_set_text(GTK_EDITABLE(state->rgb_entry), rgb);
    gtk_editable_set_text(GTK_EDITABLE(state->hsl_entry), hsl);
    gtk_editable_set_text(GTK_EDITABLE(state->oklch_entry), oklch);
    gtk_editable_set_text(GTK_EDITABLE(state->css_entry), css);
    
    char btn_txt[64];
    snprintf(btn_txt, sizeof btn_txt, "Click to copy %s", hex);
    gtk_button_set_label(GTK_BUTTON(state->copy_btn), btn_txt);
    
    char btn_css[256];
    snprintf(btn_css, sizeof btn_css, ".helvetia-color-btn { background-color: %s; color: %s; border-radius: 8px; min-height: 40px; font-weight: bold; border: none; }", 
             hex, state->v > 0.5 && state->s < 0.5 ? "black" : "white");
    GtkCssProvider *p = gtk_css_provider_new();
    gtk_css_provider_load_from_string(p, btn_css);
    gtk_style_context_add_provider(gtk_widget_get_style_context(state->copy_btn), GTK_STYLE_PROVIDER(p), 600);
    g_object_unref(p);
    
    gtk_widget_queue_draw(state->sv_area);
    gtk_widget_queue_draw(state->hue_area);
    gtk_widget_queue_draw(state->alpha_area);
    
    update_harmonies(state);
    
    state->updating = FALSE;
}

static void on_hex_changed(GtkEditable *edit, gpointer data) {
    ColorState *state = data;
    if (state->updating) return;
    
    const char *txt = gtk_editable_get_text(edit);
    if (*txt == '#') txt++;
    if (strlen(txt) == 6) {
        unsigned int r, g, b;
        if (sscanf(txt, "%02x%02x%02x", &r, &g, &b) == 3) {
            double h, s, v;
            rgb_to_hsv(r/255.0, g/255.0, b/255.0, &h, &s, &v);
            state->h = h; state->s = s; state->v = v;
            update_ui_from_state(state);
        }
    }
}

static void on_harmony_changed(GObject *obj, GParamSpec *pspec, gpointer data) {
    update_harmonies(data);
}

static void on_action_random(GtkButton *btn, gpointer data) {
    ColorState *state = data;
    state->h = rand() % 360;
    state->s = ((rand() % 100) / 100.0) * 0.8 + 0.2;
    state->v = ((rand() % 100) / 100.0) * 0.8 + 0.2;
    update_ui_from_state(state);
}

static void on_action_lighten(GtkButton *btn, gpointer data) {
    ColorState *state = data;
    state->v = fmin(1.0, state->v + 0.1);
    state->s = fmax(0.0, state->s - 0.05);
    update_ui_from_state(state);
}

static void on_action_darken(GtkButton *btn, gpointer data) {
    ColorState *state = data;
    state->v = fmax(0.0, state->v - 0.1);
    update_ui_from_state(state);
}

static void on_action_saturate(GtkButton *btn, gpointer data) {
    ColorState *state = data;
    state->s = fmin(1.0, state->s + 0.1);
    update_ui_from_state(state);
}

static void on_copy_click(GtkButton *btn, gpointer data) {
    GtkEntry *entry = data;
    GdkClipboard *cb = gdk_display_get_clipboard(gdk_display_get_default());
    gdk_clipboard_set_text(cb, gtk_editable_get_text(GTK_EDITABLE(entry)));
}

/* ------------------------------------------------------------------------- */
/* UI Builder                                                                */
/* ------------------------------------------------------------------------- */

static GtkWidget *make_value_row(const char *label, GtkWidget **entry_out) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    
    GtkWidget *lbl = gtk_label_new(label);
    gtk_widget_set_size_request(lbl, 60, -1);
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);
    gtk_widget_add_css_class(lbl, "helvetia-card-subtitle");
    
    GtkWidget *entry = gtk_entry_new();
    gtk_widget_set_hexpand(entry, TRUE);
    gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE); // default read-only
    *entry_out = entry;
    
    GtkWidget *copy = gtk_button_new_from_icon_name("edit-copy-symbolic");
    gtk_widget_add_css_class(copy, "flat");
    g_signal_connect(copy, "clicked", G_CALLBACK(on_copy_click), entry);
    
    gtk_box_append(GTK_BOX(box), lbl);
    gtk_box_append(GTK_BOX(box), entry);
    gtk_box_append(GTK_BOX(box), copy);
    return box;
}

GtkWidget *build_color_studio(void) {
    ColorState *state = g_new0(ColorState, 1);
    state->h = 21.0; state->s = 0.90; state->v = 0.91; state->a = 1.0; // Default orange
    
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 24);
    g_signal_connect_swapped(main_box, "destroy", G_CALLBACK(g_free), state);
    
    // Left column
    GtkWidget *left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_hexpand(left, TRUE);
    
    state->sv_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(state->sv_area, 300, 240);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(state->sv_area), draw_sv, state, NULL);
    
    GtkGesture *sv_drag = gtk_gesture_drag_new();
    g_signal_connect(sv_drag, "drag-update", G_CALLBACK(on_sv_drag), state);
    gtk_widget_add_controller(state->sv_area, GTK_EVENT_CONTROLLER(sv_drag));
    GtkGesture *sv_click = gtk_gesture_click_new();
    g_signal_connect(sv_click, "pressed", G_CALLBACK(on_sv_pressed), state);
    gtk_widget_add_controller(state->sv_area, GTK_EVENT_CONTROLLER(sv_click));
    
    state->hue_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(state->hue_area, 300, 24);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(state->hue_area), draw_hue, state, NULL);
    
    GtkGesture *hue_drag = gtk_gesture_drag_new();
    g_signal_connect(hue_drag, "drag-update", G_CALLBACK(on_hue_drag), state);
    gtk_widget_add_controller(state->hue_area, GTK_EVENT_CONTROLLER(hue_drag));
    GtkGesture *hue_click = gtk_gesture_click_new();
    g_signal_connect(hue_click, "pressed", G_CALLBACK(on_hue_pressed), state);
    gtk_widget_add_controller(state->hue_area, GTK_EVENT_CONTROLLER(hue_click));
    
    state->alpha_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(state->alpha_area, 300, 24);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(state->alpha_area), draw_alpha, state, NULL);
    
    GtkGesture *alpha_drag = gtk_gesture_drag_new();
    g_signal_connect(alpha_drag, "drag-update", G_CALLBACK(on_alpha_drag), state);
    gtk_widget_add_controller(state->alpha_area, GTK_EVENT_CONTROLLER(alpha_drag));
    GtkGesture *alpha_click = gtk_gesture_click_new();
    g_signal_connect(alpha_click, "pressed", G_CALLBACK(on_alpha_pressed), state);
    gtk_widget_add_controller(state->alpha_area, GTK_EVENT_CONTROLLER(alpha_click));
    
    state->copy_btn = gtk_button_new();
    gtk_widget_add_css_class(state->copy_btn, "helvetia-color-btn");
    
    gtk_box_append(GTK_BOX(left), state->sv_area);
    gtk_box_append(GTK_BOX(left), state->hue_area);
    gtk_box_append(GTK_BOX(left), state->alpha_area);
    gtk_box_append(GTK_BOX(left), state->copy_btn);
    
    // Right column
    GtkWidget *right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_size_request(right, 280, -1);
    
    GtkWidget *val_lbl = gtk_label_new("COLOR VALUES");
    gtk_widget_add_css_class(val_lbl, "helvetia-card-subtitle");
    gtk_widget_set_halign(val_lbl, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(right), val_lbl);
    
    gtk_box_append(GTK_BOX(right), make_value_row("HEX", &state->hex_entry));
    gtk_box_append(GTK_BOX(right), make_value_row("RGB", &state->rgb_entry));
    gtk_box_append(GTK_BOX(right), make_value_row("HSL", &state->hsl_entry));
    gtk_box_append(GTK_BOX(right), make_value_row("OKLCH", &state->oklch_entry));
    gtk_box_append(GTK_BOX(right), make_value_row("CSS", &state->css_entry));
    
    gtk_editable_set_editable(GTK_EDITABLE(state->hex_entry), TRUE);
    g_signal_connect(state->hex_entry, "changed", G_CALLBACK(on_hex_changed), state);
    
    gtk_box_append(GTK_BOX(right), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    
    GtkWidget *harm_lbl = gtk_label_new("COLOR HARMONY");
    gtk_widget_add_css_class(harm_lbl, "helvetia-card-subtitle");
    gtk_widget_set_halign(harm_lbl, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(right), harm_lbl);
    
    const char *harm_opts[] = {"Complementary", "Analogous", "Triadic", NULL};
    state->harmony_dd = gtk_drop_down_new_from_strings(harm_opts);
    g_signal_connect(state->harmony_dd, "notify::selected", G_CALLBACK(on_harmony_changed), state);
    gtk_box_append(GTK_BOX(right), state->harmony_dd);
    
    state->harmony_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(right), state->harmony_box);
    
    gtk_box_append(GTK_BOX(right), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    
    GtkWidget *act_lbl = gtk_label_new("QUICK ACTIONS");
    gtk_widget_add_css_class(act_lbl, "helvetia-card-subtitle");
    gtk_widget_set_halign(act_lbl, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(right), act_lbl);
    
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    
    GtkWidget *b_rand = gtk_button_new_with_label("✨ Random");
    GtkWidget *b_light = gtk_button_new_with_label("☀️ Lighten");
    GtkWidget *b_dark = gtk_button_new_with_label("🌙 Darken");
    GtkWidget *b_sat = gtk_button_new_with_label("💧 Saturate");
    
    g_signal_connect(b_rand, "clicked", G_CALLBACK(on_action_random), state);
    g_signal_connect(b_light, "clicked", G_CALLBACK(on_action_lighten), state);
    g_signal_connect(b_dark, "clicked", G_CALLBACK(on_action_darken), state);
    g_signal_connect(b_sat, "clicked", G_CALLBACK(on_action_saturate), state);
    
    gtk_grid_attach(GTK_GRID(grid), b_rand, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), b_light, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), b_dark, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), b_sat, 1, 1, 1, 1);
    
    gtk_widget_set_hexpand(b_rand, TRUE);
    gtk_widget_set_hexpand(b_light, TRUE);
    gtk_box_append(GTK_BOX(right), grid);
    
    gtk_box_append(GTK_BOX(main_box), left);
    gtk_box_append(GTK_BOX(main_box), gtk_separator_new(GTK_ORIENTATION_VERTICAL));
    gtk_box_append(GTK_BOX(main_box), right);
    
    update_ui_from_state(state);
    
    return main_box;
}
