#include "templates.h"
#include <adwaita.h>

typedef struct {
    HelvetiaViewerConfig config;
    gpointer tool_state;
    
    GtkWidget *stack;
    GtkWidget *drop_zone;
    GtkWidget *editor_widget;
    GtkWidget *actions_box;
    char *current_file;
} ViewerState;

static void free_state(gpointer data) {
    ViewerState *st = data;
    g_free(st->current_file);
    if (st->tool_state) g_free(st->tool_state);
    g_free(st);
}

static void on_file_selected(GObject *source, GAsyncResult *res, gpointer user_data) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source);
    ViewerState *st = user_data;
    
    GFile *file = gtk_file_dialog_open_finish(dialog, res, NULL);
    if (!file) return;
    
    g_free(st->current_file);
    st->current_file = g_file_get_path(file);
    g_object_unref(file);
    
    /* Switch stack */
    gtk_stack_set_visible_child(GTK_STACK(st->stack), st->editor_widget);
    
    /* Show actions box */
    gtk_widget_set_visible(st->actions_box, TRUE);
    
    /* Call tool callback if provided */
    if (st->config.on_file_loaded) {
        st->config.on_file_loaded(st->current_file, st->tool_state);
    }
}

static void on_select_file_clicked(GtkButton *btn, gpointer user_data) {
    ViewerState *st = user_data;
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Open File");
    
    GtkWindow *window = GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(btn)));
    gtk_file_dialog_open(dialog, window, NULL, on_file_selected, st);
    g_object_unref(dialog);
}

GtkWidget *helvetia_template_viewer_new(const HelvetiaViewerConfig *config) {
    ViewerState *st = g_new0(ViewerState, 1);
    st->config = *config;
    
    if (config->state_size > 0) {
        st->tool_state = g_malloc0(config->state_size);
    }
    
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    
    /* Stack for empty vs loaded state */
    st->stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(st->stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_widget_set_vexpand(st->stack, TRUE);
    gtk_box_append(GTK_BOX(box), st->stack);
    
    /* 1. Drop Zone (Empty State) */
    st->drop_zone = adw_status_page_new();
    adw_status_page_set_icon_name(ADW_STATUS_PAGE(st->drop_zone), "document-open-symbolic");
    adw_status_page_set_title(ADW_STATUS_PAGE(st->drop_zone), "Open a file");
    
    char *desc = g_strdup_printf("%s", config->file_type_hint ? config->file_type_hint : "Any file");
    adw_status_page_set_description(ADW_STATUS_PAGE(st->drop_zone), desc);
    g_free(desc);
    
    GtkWidget *sel_btn = gtk_button_new_with_label("Choose File…");
    gtk_widget_add_css_class(sel_btn, "pill");
    gtk_widget_add_css_class(sel_btn, "suggested-action");
    gtk_widget_set_halign(sel_btn, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(sel_btn, 16);
    g_signal_connect(sel_btn, "clicked", G_CALLBACK(on_select_file_clicked), st);
    adw_status_page_set_child(ADW_STATUS_PAGE(st->drop_zone), sel_btn);
    
    /* 2. Editor Widget */
    if (config->create_editor_widget) {
        st->editor_widget = config->create_editor_widget();
    } else {
        st->editor_widget = gtk_label_new("Editor Not Implemented");
    }
    
    gtk_stack_add_named(GTK_STACK(st->stack), st->drop_zone, "empty");
    gtk_stack_add_named(GTK_STACK(st->stack), st->editor_widget, "loaded");
    
    /* Actions Box (hidden by default) */
    st->actions_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(st->actions_box, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_bottom(st->actions_box, 16);
    gtk_widget_set_visible(st->actions_box, FALSE);
    gtk_box_append(GTK_BOX(box), st->actions_box);
    
    /* Save state to view */
    g_object_set_data_full(G_OBJECT(box), "viewer-state", st, free_state);
    
    if (st->tool_state) {
        g_object_set_data(G_OBJECT(box), "tool-state", st->tool_state);
    }
    g_object_set_data(G_OBJECT(box), "actions-box", st->actions_box);
    
    return box;
}
