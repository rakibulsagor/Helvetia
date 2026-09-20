#include "templates.h"
#include <adwaita.h>

typedef struct {
    HelvetiaFileProcessorConfig config;
    gpointer tool_state;
    
    GtkWidget *stack;
    GtkWidget *drop_zone;
    GtkWidget *loaded_page;
    GtkWidget *file_label;
    GtkWidget *file_size_label;
    GtkWidget *action_btn;
    GtkWidget *preview_box;
    
    char *current_file;
} FileProcessorState;

static void free_state(gpointer data) {
    FileProcessorState *st = data;
    g_free(st->current_file);
    if (st->tool_state) g_free(st->tool_state);
    g_free(st);
}

static void on_remove_file(GtkButton *btn, gpointer user_data) {
    (void)btn;
    FileProcessorState *st = user_data;
    g_clear_pointer(&st->current_file, g_free);
    gtk_stack_set_visible_child(GTK_STACK(st->stack), st->drop_zone);
}

static void on_action_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    FileProcessorState *st = user_data;
    if (!st->config.tool_id || !st->config.primary_action_id) return;
    
    GtkRoot *window = gtk_widget_get_root(GTK_WIDGET(btn));
    if (!window) return;
    
    /* Fire win.tool_action(tool_id, action_id) */
    g_action_group_activate_action(G_ACTION_GROUP(window), 
                                   "win.tool_action", 
                                   g_variant_new("(ss)", st->config.tool_id, st->config.primary_action_id));
}

static void on_file_selected(GObject *source, GAsyncResult *res, gpointer user_data) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source);
    FileProcessorState *st = user_data;
    
    GFile *file = gtk_file_dialog_open_finish(dialog, res, NULL);
    if (!file) return;
    
    g_free(st->current_file);
    st->current_file = g_file_get_path(file);
    
    /* Get file size */
    GFileInfo *info = g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_SIZE, 
                                        G_FILE_QUERY_INFO_NONE, NULL, NULL);
    if (info) {
        char *size_str = g_format_size(g_file_info_get_size(info));
        gtk_label_set_text(GTK_LABEL(st->file_size_label), size_str);
        g_free(size_str);
        g_object_unref(info);
    }
    
    /* Get file name */
    char *basename = g_file_get_basename(file);
    gtk_label_set_text(GTK_LABEL(st->file_label), basename);
    g_free(basename);
    g_object_unref(file);
    
    /* Switch stack */
    gtk_stack_set_visible_child(GTK_STACK(st->stack), st->loaded_page);
    
    /* Call tool callback if provided */
    if (st->config.on_file_loaded) {
        st->config.on_file_loaded(st->current_file, st->tool_state);
    }
}

static void on_select_file_clicked(GtkButton *btn, gpointer user_data) {
    FileProcessorState *st = user_data;
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Select File");
    
    GtkWindow *window = GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(btn)));
    gtk_file_dialog_open(dialog, window, NULL, on_file_selected, st);
    g_object_unref(dialog);
}

GtkWidget *helvetia_template_file_processor_new(const HelvetiaFileProcessorConfig *config) {
    FileProcessorState *st = g_new0(FileProcessorState, 1);
    st->config = *config;
    
    if (config->state_size > 0) {
        st->tool_state = g_malloc0(config->state_size);
    }
    
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    
    /* Stack for empty vs loaded state */
    st->stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(st->stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_widget_set_vexpand(st->stack, TRUE);
    
    /* 1. Drop Zone (Empty State) */
    st->drop_zone = adw_status_page_new();
    adw_status_page_set_icon_name(ADW_STATUS_PAGE(st->drop_zone), "document-open-symbolic");
    adw_status_page_set_title(ADW_STATUS_PAGE(st->drop_zone), "Select a file");
    
    char *desc = g_strdup_printf("%s\nYour files never leave your device.", 
                                 config->file_type_hint ? config->file_type_hint : "Any file");
    adw_status_page_set_description(ADW_STATUS_PAGE(st->drop_zone), desc);
    g_free(desc);
    
    GtkWidget *sel_btn = gtk_button_new_with_label("Choose File…");
    gtk_widget_add_css_class(sel_btn, "pill");
    gtk_widget_add_css_class(sel_btn, "suggested-action");
    gtk_widget_set_halign(sel_btn, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(sel_btn, 16);
    g_signal_connect(sel_btn, "clicked", G_CALLBACK(on_select_file_clicked), st);
    adw_status_page_set_child(ADW_STATUS_PAGE(st->drop_zone), sel_btn);
    
    /* 2. Loaded Page */
    st->loaded_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start(st->loaded_page, 24);
    gtk_widget_set_margin_end(st->loaded_page, 24);
    gtk_widget_set_margin_top(st->loaded_page, 24);
    gtk_widget_set_margin_bottom(st->loaded_page, 24);
    
    /* File Card */
    GtkWidget *file_card = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(file_card, "card");
    gtk_widget_set_margin_bottom(file_card, 16);
    
    GtkWidget *file_icon = gtk_image_new_from_icon_name("text-x-generic-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(file_icon), 32);
    st->file_label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(st->file_label), 0.0);
    gtk_label_set_ellipsize(GTK_LABEL(st->file_label), PANGO_ELLIPSIZE_MIDDLE);
    gtk_widget_set_hexpand(st->file_label, TRUE);
    
    st->file_size_label = gtk_label_new("");
    gtk_widget_add_css_class(st->file_size_label, "dim-label");
    
    GtkWidget *remove_btn = gtk_button_new_from_icon_name("window-close-symbolic");
    gtk_widget_add_css_class(remove_btn, "flat");
    g_signal_connect(remove_btn, "clicked", G_CALLBACK(on_remove_file), st);
    
    gtk_box_append(GTK_BOX(file_card), file_icon);
    gtk_box_append(GTK_BOX(file_card), st->file_label);
    gtk_box_append(GTK_BOX(file_card), st->file_size_label);
    gtk_box_append(GTK_BOX(file_card), remove_btn);
    
    gtk_box_append(GTK_BOX(st->loaded_page), file_card);
    
    /* Options Widget */
    if (config->create_options_widget) {
        GtkWidget *options = config->create_options_widget();
        gtk_box_append(GTK_BOX(st->loaded_page), options);
    }
    
    /* Primary Action */
    if (config->primary_action_id && config->primary_action_label) {
        st->action_btn = gtk_button_new_with_label(config->primary_action_label);
        gtk_widget_add_css_class(st->action_btn, "suggested-action");
        gtk_widget_add_css_class(st->action_btn, "pill");
        gtk_widget_set_halign(st->action_btn, GTK_ALIGN_CENTER);
        gtk_widget_set_margin_top(st->action_btn, 16);
        g_signal_connect(st->action_btn, "clicked", G_CALLBACK(on_action_clicked), st);
        gtk_box_append(GTK_BOX(st->loaded_page), st->action_btn);
    }
    
    /* Preview Box */
    st->preview_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(st->preview_box, TRUE);
    gtk_widget_set_margin_top(st->preview_box, 24);
    gtk_box_append(GTK_BOX(st->loaded_page), st->preview_box);
    
    gtk_stack_add_named(GTK_STACK(st->stack), st->drop_zone, "empty");
    gtk_stack_add_named(GTK_STACK(st->stack), st->loaded_page, "loaded");
    
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), st->stack);
    gtk_widget_set_vexpand(scroll, TRUE);
    
    gtk_box_append(GTK_BOX(box), scroll);
    
    /* Save state to view */
    g_object_set_data_full(G_OBJECT(box), "file-processor-state", st, free_state);
    
    /* Expose tool_state so tool can retrieve it */
    if (st->tool_state) {
        g_object_set_data(G_OBJECT(box), "tool-state", st->tool_state);
    }
    
    /* Expose preview box so tool can pack things into it later */
    g_object_set_data(G_OBJECT(box), "preview-box", st->preview_box);
    
    return box;
}
