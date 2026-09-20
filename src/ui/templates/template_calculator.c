#include "templates.h"
#include <adwaita.h>

typedef struct {
    HelvetiaCalculatorConfig config;
    gpointer tool_state;
    
    GtkWidget *result_box;
    GtkWidget *actions_box;
} CalculatorState;

static void free_state(gpointer data) {
    CalculatorState *st = data;
    if (st->tool_state) g_free(st->tool_state);
    g_free(st);
}

GtkWidget *helvetia_template_calculator_new(const HelvetiaCalculatorConfig *config) {
    CalculatorState *st = g_new0(CalculatorState, 1);
    st->config = *config;
    
    if (config->state_size > 0) {
        st->tool_state = g_malloc0(config->state_size);
    }
    
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start(main_box, 24);
    gtk_widget_set_margin_end(main_box, 24);
    gtk_widget_set_margin_top(main_box, 24);
    gtk_widget_set_margin_bottom(main_box, 24);
    
    /* Fields Widget */
    if (config->create_fields_widget) {
        GtkWidget *fields = config->create_fields_widget();
        gtk_box_append(GTK_BOX(main_box), fields);
    }
    
    /* Result Section */
    GtkWidget *result_lbl = gtk_label_new("Result:");
    gtk_widget_set_halign(result_lbl, GTK_ALIGN_START);
    gtk_widget_add_css_class(result_lbl, "heading");
    gtk_box_append(GTK_BOX(main_box), result_lbl);
    
    st->result_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(st->result_box, "card");
    gtk_widget_set_vexpand(st->result_box, TRUE);
    gtk_box_append(GTK_BOX(main_box), st->result_box);
    
    /* Actions Box */
    st->actions_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(st->actions_box, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(st->actions_box, 12);
    gtk_box_append(GTK_BOX(main_box), st->actions_box);
    
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), main_box);
    gtk_widget_set_vexpand(scroll, TRUE);
    
    /* Save state */
    g_object_set_data_full(G_OBJECT(scroll), "calculator-state", st, free_state);
    
    if (st->tool_state) {
        g_object_set_data(G_OBJECT(scroll), "tool-state", st->tool_state);
    }
    g_object_set_data(G_OBJECT(scroll), "result-box", st->result_box);
    g_object_set_data(G_OBJECT(scroll), "actions-box", st->actions_box);
    
    return scroll;
}
