#include "templates.h"
#include <adwaita.h>

typedef struct {
    HelvetiaLookupConfig config;
    gpointer tool_state;
    
    GtkWidget *query_entry;
    GtkWidget *action_btn;
    GtkWidget *results_box;
    GtkWidget *actions_box;
} LookupState;

static void free_state(gpointer data) {
    LookupState *st = data;
    if (st->tool_state) g_free(st->tool_state);
    g_free(st);
}

static void on_action_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    LookupState *st = user_data;
    if (!st->config.tool_id || !st->config.primary_action_id) return;
    
    GtkRoot *window = gtk_widget_get_root(GTK_WIDGET(btn));
    if (!window) return;
    
    g_action_group_activate_action(G_ACTION_GROUP(window), 
                                   "win.tool_action", 
                                   g_variant_new("(ss)", st->config.tool_id, st->config.primary_action_id));
}

static void on_entry_activated(GtkEntry *entry, gpointer user_data) {
    (void)entry;
    LookupState *st = user_data;
    if (st->action_btn) {
        on_action_clicked(GTK_BUTTON(st->action_btn), st);
    }
}

GtkWidget *helvetia_template_lookup_new(const HelvetiaLookupConfig *config) {
    LookupState *st = g_new0(LookupState, 1);
    st->config = *config;
    
    if (config->state_size > 0) {
        st->tool_state = g_malloc0(config->state_size);
    }
    
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start(main_box, 24);
    gtk_widget_set_margin_end(main_box, 24);
    gtk_widget_set_margin_top(main_box, 24);
    gtk_widget_set_margin_bottom(main_box, 24);
    
    /* Query Box */
    GtkWidget *query_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(query_box, GTK_ALIGN_CENTER);
    
    GtkWidget *query_lbl = gtk_label_new("Query:");
    gtk_widget_add_css_class(query_lbl, "heading");
    gtk_box_append(GTK_BOX(query_box), query_lbl);
    
    st->query_entry = gtk_entry_new();
    gtk_widget_set_size_request(st->query_entry, 300, -1);
    if (config->query_placeholder) {
        gtk_entry_set_placeholder_text(GTK_ENTRY(st->query_entry), config->query_placeholder);
    }
    g_signal_connect(st->query_entry, "activate", G_CALLBACK(on_entry_activated), st);
    gtk_box_append(GTK_BOX(query_box), st->query_entry);
    
    if (config->primary_action_id) {
        st->action_btn = gtk_button_new_with_label("Search");
        gtk_widget_add_css_class(st->action_btn, "suggested-action");
        g_signal_connect(st->action_btn, "clicked", G_CALLBACK(on_action_clicked), st);
        gtk_box_append(GTK_BOX(query_box), st->action_btn);
    }
    
    gtk_box_append(GTK_BOX(main_box), query_box);
    
    /* Results Box */
    GtkWidget *results_lbl = gtk_label_new("Results:");
    gtk_widget_set_halign(results_lbl, GTK_ALIGN_START);
    gtk_widget_add_css_class(results_lbl, "heading");
    gtk_box_append(GTK_BOX(main_box), results_lbl);
    
    st->results_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(st->results_box, "card");
    gtk_widget_set_vexpand(st->results_box, TRUE);
    gtk_box_append(GTK_BOX(main_box), st->results_box);
    
    /* Actions Box */
    st->actions_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(st->actions_box, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(main_box), st->actions_box);
    
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), main_box);
    gtk_widget_set_vexpand(scroll, TRUE);
    
    /* Save state */
    g_object_set_data_full(G_OBJECT(scroll), "lookup-state", st, free_state);
    
    if (st->tool_state) {
        g_object_set_data(G_OBJECT(scroll), "tool-state", st->tool_state);
    }
    g_object_set_data(G_OBJECT(scroll), "query-entry", st->query_entry);
    g_object_set_data(G_OBJECT(scroll), "results-box", st->results_box);
    g_object_set_data(G_OBJECT(scroll), "actions-box", st->actions_box);
    
    return scroll;
}
