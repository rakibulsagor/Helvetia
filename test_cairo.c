#include <gtk/gtk.h>
#include <math.h>

static void draw_sv(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer data) {
    // Background hue (e.g. red)
    cairo_set_source_rgb(cr, 1, 0, 0);
    cairo_paint(cr);

    // White to transparent (Saturation)
    cairo_pattern_t *sat = cairo_pattern_create_linear(0, 0, width, 0);
    cairo_pattern_add_color_stop_rgba(sat, 0, 1, 1, 1, 1);
    cairo_pattern_add_color_stop_rgba(sat, 1, 1, 1, 1, 0);
    cairo_set_source(cr, sat);
    cairo_paint(cr);
    cairo_pattern_destroy(sat);

    // Transparent to black (Value)
    cairo_pattern_t *val = cairo_pattern_create_linear(0, 0, 0, height);
    cairo_pattern_add_color_stop_rgba(val, 0, 0, 0, 0, 0);
    cairo_pattern_add_color_stop_rgba(val, 1, 0, 0, 0, 1);
    cairo_set_source(cr, val);
    cairo_paint(cr);
    cairo_pattern_destroy(val);
}

static void app_activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *win = gtk_application_window_new(app);
    GtkWidget *area = gtk_drawing_area_new();
    gtk_widget_set_size_request(area, 300, 200);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(area), draw_sv, NULL, NULL);
    
    gtk_window_set_child(GTK_WINDOW(win), area);
    gtk_window_present(GTK_WINDOW(win));
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("org.gtk.testcairo", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(app_activate), NULL);
    return g_application_run(G_APPLICATION(app), argc, argv);
}
