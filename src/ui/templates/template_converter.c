#include "templates.h"
#include <adwaita.h>

typedef struct {
    HelvetiaConverterConfig config;
    gpointer tool_state;
    
    GtkWidget *input_box;
    GtkWidget *output_box;
    GtkWidget *action_btn;
    GtkWidget *actions_box;
} ConverterState;

static void free_state(gpointer data) {
    ConverterState *st = data;
    if (st->tool_state) g_free(st->tool_state);
    g_free(st);
}

static void on_action_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    ConverterState *st = user_data;
    if (!st->config.tool_id || !st->config.primary_action_id) return;
    
    GtkRoot *window = gtk_widget_get_root(GTK_WIDGET(btn));
    if (!window) return;
    
    g_action_group_activate_action(G_ACTION_GROUP(window), 
                                   "win.tool_action", 
                                   g_variant_new("(ss)", st->config.tool_id, st->config.primary_action_id));
}

GtkWidget *helvetia_template_converter_new(const HelvetiaConverterConfig *config) {
    ConverterState *st = g_new0(ConverterState, 1);
    st->config = *config;
    
    if (config->state_size > 0) {
        st->tool_state = g_malloc0(config->state_size);
    }
    
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start(main_box, 24);
    gtk_widget_set_margin_end(main_box, 24);
    gtk_widget_set_margin_top(main_box, 24);
    gtk_widget_set_margin_bottom(main_box, 24);
    
    /* Input/Output Split */
    GtkWidget *split_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_set_vexpand(split_box, TRUE);
    gtk_box_append(GTK_BOX(main_box), split_box);
    
    /* Input Box */
    GtkWidget *input_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_hexpand(input_container, TRUE);
    GtkWidget *input_lbl = gtk_label_new("INPUT");
    gtk_widget_set_halign(input_lbl, GTK_ALIGN_START);
    gtk_widget_add_css_class(input_lbl, "heading");
    gtk_box_append(GTK_BOX(input_container), input_lbl);
    
    st->input_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(st->input_box, "card");
    gtk_widget_set_vexpand(st->input_box, TRUE);
    gtk_box_append(GTK_BOX(input_container), st->input_box);
    gtk_box_append(GTK_BOX(split_box), input_container);
    
    /* Middle divider/button */
    GtkWidget *mid_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_valign(mid_box, GTK_ALIGN_CENTER);
    
    if (!config->is_live && config->primary_action_id && config->primary_action_label) {
        st->action_btn = gtk_button_new_with_label(config->primary_action_label);
        gtk_widget_add_css_class(st->action_btn, "suggested-action");
        gtk_widget_add_css_class(st->action_btn, "pill");
        g_signal_connect(st->action_btn, "clicked", G_CALLBACK(on_action_clicked), st);
        gtk_box_append(GTK_BOX(mid_box), st->action_btn);
    } else {
        GtkWidget *arrow = gtk_image_new_from_icon_name("go-next-symbolic");
        gtk_image_set_pixel_size(GTK_IMAGE(arrow), 24);
        gtk_widget_add_css_class(arrow, "dim-label");
        gtk_box_append(GTK_BOX(mid_box), arrow);
    }
    gtk_box_append(GTK_BOX(split_box), mid_box);
    
    /* Output Box */
    GtkWidget *output_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_hexpand(output_container, TRUE);
    GtkWidget *output_lbl = gtk_label_new("OUTPUT");
    gtk_widget_set_halign(output_lbl, GTK_ALIGN_START);
    gtk_widget_add_css_class(output_lbl, "heading");
    gtk_box_append(GTK_BOX(output_container), output_lbl);
    
    st->output_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(st->output_box, "card");
    gtk_widget_set_vexpand(st->output_box, TRUE);
    gtk_box_append(GTK_BOX(output_container), st->output_box);
    gtk_box_append(GTK_BOX(split_box), output_container);
    
    /* Options Widget */
    if (config->create_options_widget) {
        GtkWidget *options = config->create_options_widget();
        gtk_box_append(GTK_BOX(main_box), options);
    }
    
    /* Bottom Actions Box */
    st->actions_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(st->actions_box, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(main_box), st->actions_box);
    
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), main_box);
    gtk_widget_set_vexpand(scroll, TRUE);
    
    /* Save state */
    g_object_set_data_full(G_OBJECT(scroll), "converter-state", st, free_state);
    
    if (st->tool_state) {
        g_object_set_data(G_OBJECT(scroll), "tool-state", st->tool_state);
    }
    g_object_set_data(G_OBJECT(scroll), "input-box", st->input_box);
    g_object_set_data(G_OBJECT(scroll), "output-box", st->output_box);
    g_object_set_data(G_OBJECT(scroll), "actions-box", st->actions_box);
    
    return scroll;
}
