#include "time_picker.h"
#include <math.h>

#define M_PI 3.14159265358979323846

typedef enum {
    MODE_HOUR,
    MODE_MINUTE
} TimePickerMode;

typedef struct {
    GtkWidget *dialog;
    GtkWidget *drawing_area;
    GtkWidget *lbl_hour;
    GtkWidget *lbl_sep;
    GtkWidget *lbl_minute;
    GtkWidget *lbl_am;
    GtkWidget *lbl_pm;

    int hour;       // 1-12
    int minute;     // 0-59
    gboolean is_am; // TRUE for AM, FALSE for PM
    
    TimePickerMode mode;
    TimePickerCallback callback;
    gpointer user_data;
} TimePickerState;

static void update_header_ui(TimePickerState *state) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d", state->hour);
    gtk_label_set_text(GTK_LABEL(state->lbl_hour), buf);

    snprintf(buf, sizeof(buf), "%02d", state->minute);
    gtk_label_set_text(GTK_LABEL(state->lbl_minute), buf);
    
    // Styling based on mode
    if (state->mode == MODE_HOUR) {
        gtk_widget_add_css_class(state->lbl_hour, "accent");
        gtk_widget_remove_css_class(state->lbl_minute, "accent");
    } else {
        gtk_widget_add_css_class(state->lbl_minute, "accent");
        gtk_widget_remove_css_class(state->lbl_hour, "accent");
    }

    if (state->is_am) {
        gtk_widget_add_css_class(state->lbl_am, "accent");
        gtk_widget_remove_css_class(state->lbl_pm, "accent");
    } else {
        gtk_widget_add_css_class(state->lbl_pm, "accent");
        gtk_widget_remove_css_class(state->lbl_am, "accent");
    }
}

static void draw_clock_face(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer data) {
    TimePickerState *state = data;
    double cx = width / 2.0;
    double cy = height / 2.0;
    double radius = MIN(width, height) / 2.0 - 20.0;

    // Background circle
    cairo_set_source_rgba(cr, 0.2, 0.2, 0.2, 1.0); // Dark grey
    cairo_arc(cr, cx, cy, radius, 0, 2 * M_PI);
    cairo_fill(cr);

    // Get accent color (we'll just use a fixed color or read from theme if possible, but fixed is easier)
    // Gold/Yellow accent similar to the screenshot: RGB(200, 160, 50) roughly
    double acc_r = 0.8, acc_g = 0.6, acc_b = 0.2;

    int current_val = (state->mode == MODE_HOUR) ? state->hour : state->minute;
    double angle = 0;
    if (state->mode == MODE_HOUR) {
        angle = (current_val % 12) * (M_PI / 6.0) - M_PI / 2.0;
    } else {
        angle = current_val * (M_PI / 30.0) - M_PI / 2.0;
    }

    double pointer_r = radius - 30.0;
    double px = cx + cos(angle) * pointer_r;
    double py = cy + sin(angle) * pointer_r;

    // Draw connecting line
    cairo_set_line_width(cr, 2.0);
    cairo_set_source_rgba(cr, acc_r, acc_g, acc_b, 1.0);
    cairo_move_to(cr, cx, cy);
    cairo_line_to(cr, px, py);
    cairo_stroke(cr);

    // Center dot
    cairo_arc(cr, cx, cy, 4.0, 0, 2 * M_PI);
    cairo_fill(cr);

    // Draw selected circle
    cairo_arc(cr, px, py, 18.0, 0, 2 * M_PI);
    cairo_fill(cr);
    
    // Draw numbers
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 14.0);

    for (int i = 1; i <= 12; i++) {
        double num_angle = i * (M_PI / 6.0) - M_PI / 2.0;
        double nx = cx + cos(num_angle) * pointer_r;
        double ny = cy + sin(num_angle) * pointer_r;

        char num_str[4];
        if (state->mode == MODE_HOUR) {
            snprintf(num_str, sizeof(num_str), "%d", i);
        } else {
            snprintf(num_str, sizeof(num_str), "%02d", (i * 5) % 60);
        }

        cairo_text_extents_t ext;
        cairo_text_extents(cr, num_str, &ext);

        // if it's the selected number, draw it in contrasting color
        if (state->mode == MODE_HOUR && i == state->hour || state->mode == MODE_HOUR && (i == 12 && state->hour == 12)) {
             cairo_set_source_rgba(cr, 0.1, 0.1, 0.1, 1.0); // dark text inside circle
        } else if (state->mode == MODE_MINUTE && (i * 5) % 60 == state->minute) {
             cairo_set_source_rgba(cr, 0.1, 0.1, 0.1, 1.0);
        } else {
             cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0); // white text
        }
        
        cairo_move_to(cr, nx - ext.width / 2.0, ny + ext.height / 2.0);
        cairo_show_text(cr, num_str);
    }
}

static void update_time_from_coordinates(TimePickerState *state, double x, double y) {
    int width = gtk_widget_get_width(state->drawing_area);
    int height = gtk_widget_get_height(state->drawing_area);
    double cx = width / 2.0;
    double cy = height / 2.0;
    
    double dx = x - cx;
    double dy = y - cy;
    double angle = atan2(dy, dx) + M_PI / 2.0;
    if (angle < 0) angle += 2 * M_PI;

    if (state->mode == MODE_HOUR) {
        int h = (int)round(angle / (M_PI / 6.0));
        if (h == 0) h = 12;
        state->hour = h;
    } else {
        int m = (int)round(angle / (M_PI / 30.0));
        if (m >= 60) m = 0;
        state->minute = m;
    }

    update_header_ui(state);
    gtk_widget_queue_draw(state->drawing_area);
}

static void on_drag_update(GtkGestureDrag *gesture, double offset_x, double offset_y, gpointer data) {
    TimePickerState *state = data;
    double start_x, start_y;
    gtk_gesture_drag_get_start_point(gesture, &start_x, &start_y);
    update_time_from_coordinates(state, start_x + offset_x, start_y + offset_y);
}

static void on_click_pressed(GtkGestureClick *gesture, int n_press, double x, double y, gpointer data) {
    TimePickerState *state = data;
    update_time_from_coordinates(state, x, y);
}

static void on_click_released(GtkGestureClick *gesture, int n_press, double x, double y, gpointer data) {
    TimePickerState *state = data;
    if (state->mode == MODE_HOUR) {
        // Auto transition to minute selection
        state->mode = MODE_MINUTE;
        update_header_ui(state);
        gtk_widget_queue_draw(state->drawing_area);
    }
}

static void on_hour_clicked(GtkGestureClick *gesture, int n_press, double x, double y, gpointer data) {
    TimePickerState *state = data;
    state->mode = MODE_HOUR;
    update_header_ui(state);
    gtk_widget_queue_draw(state->drawing_area);
}

static void on_minute_clicked(GtkGestureClick *gesture, int n_press, double x, double y, gpointer data) {
    TimePickerState *state = data;
    state->mode = MODE_MINUTE;
    update_header_ui(state);
    gtk_widget_queue_draw(state->drawing_area);
}

static void on_am_clicked(GtkGestureClick *gesture, int n_press, double x, double y, gpointer data) {
    TimePickerState *state = data;
    state->is_am = TRUE;
    update_header_ui(state);
}

static void on_pm_clicked(GtkGestureClick *gesture, int n_press, double x, double y, gpointer data) {
    TimePickerState *state = data;
    state->is_am = FALSE;
    update_header_ui(state);
}

static void on_cancel_clicked(GtkButton *btn, gpointer data) {
    TimePickerState *state = data;
    gtk_window_destroy(GTK_WINDOW(state->dialog));
}

static void on_ok_clicked(GtkButton *btn, gpointer data) {
    TimePickerState *state = data;
    if (state->callback) {
        state->callback(state->hour, state->minute, state->is_am, state->user_data);
    }
    gtk_window_destroy(GTK_WINDOW(state->dialog));
}

static void on_dialog_destroy(GtkWidget *widget, gpointer data) {
    TimePickerState *state = data;
    g_free(state);
}

void show_time_picker(GtkWindow *parent,
                      int initial_hour,
                      int initial_minute,
                      TimePickerCallback cb,
                      gpointer user_data) {
    TimePickerState *state = g_new0(TimePickerState, 1);
    
    // Normalize time
    state->is_am = (initial_hour < 12);
    state->hour = initial_hour % 12;
    if (state->hour == 0) state->hour = 12;
    state->minute = initial_minute;
    state->mode = MODE_HOUR;
    state->callback = cb;
    state->user_data = user_data;

    state->dialog = gtk_window_new();
    if (parent) {
        gtk_window_set_transient_for(GTK_WINDOW(state->dialog), parent);
        gtk_window_set_modal(GTK_WINDOW(state->dialog), TRUE);
    }
    gtk_window_set_title(GTK_WINDOW(state->dialog), "Select Time");
    gtk_window_set_default_size(GTK_WINDOW(state->dialog), 320, 480);
    g_signal_connect(state->dialog, "destroy", G_CALLBACK(on_dialog_destroy), state);

    // CSS provider for large font
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_string(css, 
        ".time-header { font-size: 64px; font-weight: bold; }\n"
        ".ampm-label { font-size: 20px; font-weight: bold; }\n"
        ".accent { color: #f2a900; }\n"
        ".dialog-bg { background-color: #2b2b2b; }\n"
    );
    gtk_style_context_add_provider_for_display(gdk_display_get_default(),
        GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_USER);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(vbox, "dialog-bg");
    gtk_window_set_child(GTK_WINDOW(state->dialog), vbox);

    // Header section (darker bg like screenshot)
    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_set_margin_top(header_box, 32);
    gtk_widget_set_margin_bottom(header_box, 32);
    gtk_widget_set_halign(header_box, GTK_ALIGN_CENTER);

    state->lbl_hour = gtk_label_new("");
    gtk_widget_add_css_class(state->lbl_hour, "time-header");
    GtkGesture *h_click = gtk_gesture_click_new();
    g_signal_connect(h_click, "pressed", G_CALLBACK(on_hour_clicked), state);
    gtk_widget_add_controller(state->lbl_hour, GTK_EVENT_CONTROLLER(h_click));

    state->lbl_sep = gtk_label_new(":");
    gtk_widget_add_css_class(state->lbl_sep, "time-header");

    state->lbl_minute = gtk_label_new("");
    gtk_widget_add_css_class(state->lbl_minute, "time-header");
    GtkGesture *m_click = gtk_gesture_click_new();
    g_signal_connect(m_click, "pressed", G_CALLBACK(on_minute_clicked), state);
    gtk_widget_add_controller(state->lbl_minute, GTK_EVENT_CONTROLLER(m_click));

    GtkWidget *ampm_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_valign(ampm_box, GTK_ALIGN_CENTER);
    
    state->lbl_am = gtk_label_new("AM");
    gtk_widget_add_css_class(state->lbl_am, "ampm-label");
    GtkGesture *am_click = gtk_gesture_click_new();
    g_signal_connect(am_click, "pressed", G_CALLBACK(on_am_clicked), state);
    gtk_widget_add_controller(state->lbl_am, GTK_EVENT_CONTROLLER(am_click));

    state->lbl_pm = gtk_label_new("PM");
    gtk_widget_add_css_class(state->lbl_pm, "ampm-label");
    GtkGesture *pm_click = gtk_gesture_click_new();
    g_signal_connect(pm_click, "pressed", G_CALLBACK(on_pm_clicked), state);
    gtk_widget_add_controller(state->lbl_pm, GTK_EVENT_CONTROLLER(pm_click));

    gtk_box_append(GTK_BOX(ampm_box), state->lbl_am);
    gtk_box_append(GTK_BOX(ampm_box), state->lbl_pm);

    gtk_box_append(GTK_BOX(header_box), state->lbl_hour);
    gtk_box_append(GTK_BOX(header_box), state->lbl_sep);
    gtk_box_append(GTK_BOX(header_box), state->lbl_minute);
    gtk_box_append(GTK_BOX(header_box), ampm_box);

    gtk_box_append(GTK_BOX(vbox), header_box);

    // Clock Face
    state->drawing_area = gtk_drawing_area_new();
    gtk_widget_set_vexpand(state->drawing_area, TRUE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(state->drawing_area), draw_clock_face, state, NULL);
    
    GtkGesture *drag = gtk_gesture_drag_new();
    g_signal_connect(drag, "drag-update", G_CALLBACK(on_drag_update), state);
    gtk_widget_add_controller(state->drawing_area, GTK_EVENT_CONTROLLER(drag));

    GtkGesture *click = gtk_gesture_click_new();
    g_signal_connect(click, "pressed", G_CALLBACK(on_click_pressed), state);
    g_signal_connect(click, "released", G_CALLBACK(on_click_released), state);
    gtk_widget_add_controller(state->drawing_area, GTK_EVENT_CONTROLLER(click));

    gtk_box_append(GTK_BOX(vbox), state->drawing_area);

    // Buttons
    GtkWidget *action_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_set_margin_start(action_box, 16);
    gtk_widget_set_margin_end(action_box, 16);
    gtk_widget_set_margin_bottom(action_box, 16);
    gtk_widget_set_halign(action_box, GTK_ALIGN_END);

    GtkWidget *btn_cancel = gtk_button_new_with_label("CANCEL");
    gtk_widget_add_css_class(btn_cancel, "flat");
    gtk_widget_add_css_class(btn_cancel, "accent");
    g_signal_connect(btn_cancel, "clicked", G_CALLBACK(on_cancel_clicked), state);

    GtkWidget *btn_ok = gtk_button_new_with_label("OK");
    gtk_widget_add_css_class(btn_ok, "flat");
    gtk_widget_add_css_class(btn_ok, "accent");
    g_signal_connect(btn_ok, "clicked", G_CALLBACK(on_ok_clicked), state);

    gtk_box_append(GTK_BOX(action_box), btn_cancel);
    gtk_box_append(GTK_BOX(action_box), btn_ok);
    gtk_box_append(GTK_BOX(vbox), action_box);

    update_header_ui(state);
    gtk_window_present(GTK_WINDOW(state->dialog));
}
