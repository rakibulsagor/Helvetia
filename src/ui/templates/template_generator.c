#include "templates.h"
#include <adwaita.h>

typedef struct {
    HelvetiaGeneratorConfig config;
    gpointer tool_state;
    
    GtkWidget *action_btn;
    GtkWidget *preview_box;
    GtkWidget *actions_box; /* For Copy / Save etc */
} GeneratorState;

static void free_state(gpointer data) {
    GeneratorState *st = data;
    if (st->tool_state) g_free(st->tool_state);
    g_free(st);
}

static void on_action_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    GeneratorState *st = user_data;
    if (!st->config.tool_id || !st->config.primary_action_id) return;
    
    GtkRoot *window = gtk_widget_get_root(GTK_WIDGET(btn));
    if (!window) return;
    
    g_action_group_activate_action(G_ACTION_GROUP(window), 
                                   "win.tool_action", 
                                   g_variant_new("(ss)", st->config.tool_id, st->config.primary_action_id));
}

GtkWidget *helvetia_template_generator_new(const HelvetiaGeneratorConfig *config) {
    GeneratorState *st = g_new0(GeneratorState, 1);
    st->config = *config;
    
    if (config->state_size > 0) {
        st->tool_state = g_malloc0(config->state_size);
    }
    
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start(box, 24);
    gtk_widget_set_margin_end(box, 24);
    gtk_widget_set_margin_top(box, 24);
    gtk_widget_set_margin_bottom(box, 24);
    
    /* Inputs Widget */
    if (config->create_inputs_widget) {
        GtkWidget *inputs = config->create_inputs_widget();
        gtk_box_append(GTK_BOX(box), inputs);
    }
    
    /* Primary Action */
    if (config->primary_action_id && config->primary_action_label) {
        st->action_btn = gtk_button_new_with_label(config->primary_action_label);
        gtk_widget_add_css_class(st->action_btn, "suggested-action");
        gtk_widget_add_css_class(st->action_btn, "pill");
        gtk_widget_set_halign(st->action_btn, GTK_ALIGN_CENTER);
        gtk_widget_set_margin_top(st->action_btn, 8);
        gtk_widget_set_margin_bottom(st->action_btn, 8);
        g_signal_connect(st->action_btn, "clicked", G_CALLBACK(on_action_clicked), st);
        gtk_box_append(GTK_BOX(box), st->action_btn);
    }
    
    /* Output Section */
    GtkWidget *output_label = gtk_label_new("Output:");
    gtk_widget_set_halign(output_label, GTK_ALIGN_START);
    gtk_widget_add_css_class(output_label, "heading");
    gtk_box_append(GTK_BOX(box), output_label);
    
    st->preview_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(st->preview_box, "card");
    gtk_widget_set_vexpand(st->preview_box, TRUE);
    gtk_box_append(GTK_BOX(box), st->preview_box);
    
    /* Bottom Actions Box */
    st->actions_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(st->actions_box, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(st->actions_box, 12);
    gtk_box_append(GTK_BOX(box), st->actions_box);
    
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), box);
    gtk_widget_set_vexpand(scroll, TRUE);
    
    /* Save state */
    g_object_set_data_full(G_OBJECT(scroll), "generator-state", st, free_state);
    
    if (st->tool_state) {
        g_object_set_data(G_OBJECT(scroll), "tool-state", st->tool_state);
    }
    g_object_set_data(G_OBJECT(scroll), "preview-box", st->preview_box);
    g_object_set_data(G_OBJECT(scroll), "actions-box", st->actions_box);
    
    return scroll;
}
