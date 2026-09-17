#include "module_view.h"

static GtkWidget *make_tool_card(const HelvetiaTool *tool) {
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_add_css_class(card, "helvetia-card");

    // Header box (Icon + Title)
    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    if (tool->icon_name) {
        GtkWidget *icon = gtk_image_new_from_icon_name(tool->icon_name);
        gtk_box_append(GTK_BOX(header_box), icon);
    }
    
    GtkWidget *title = gtk_label_new(tool->name);
    gtk_widget_add_css_class(title, "helvetia-card-title");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(header_box), title);
    
    gtk_box_append(GTK_BOX(card), header_box);

    if (tool->description && *tool->description) {
        GtkWidget *subtitle = gtk_label_new(tool->description);
        gtk_widget_add_css_class(subtitle, "helvetia-card-subtitle");
        gtk_widget_set_halign(subtitle, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(card), subtitle);
    }

    if (tool->create_view) {
        GtkWidget *content = tool->create_view();
        if (content) {
            gtk_box_append(GTK_BOX(card), content);
        }
    } else {
        GtkWidget *placeholder = gtk_label_new("Coming Soon");
        gtk_widget_add_css_class(placeholder, "helvetia-fg-muted");
        gtk_widget_set_valign(placeholder, GTK_ALIGN_CENTER);
        gtk_widget_set_halign(placeholder, GTK_ALIGN_CENTER);
        gtk_widget_set_vexpand(placeholder, TRUE);
        gtk_box_append(GTK_BOX(card), placeholder);
    }

    return card;
}

GtkWidget *helvetia_module_view_new(const HelvetiaModule *module) {
    if (!module || !module->subcategories) return NULL;

    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 24);
    gtk_widget_set_margin_start(main_box, 24);
    gtk_widget_set_margin_end(main_box, 24);
    gtk_widget_set_margin_top(main_box, 24);
    gtk_widget_set_margin_bottom(main_box, 24);
    
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), main_box);

    for (int i = 0; module->subcategories[i].name != NULL; i++) {
        const HelvetiaSubcategory *sub = &module->subcategories[i];
        
        // Category Header
        GtkWidget *cat_label = gtk_label_new(sub->name);
        gtk_widget_set_halign(cat_label, GTK_ALIGN_START);
        // We can reuse or define a new CSS class for category headers
        gtk_widget_add_css_class(cat_label, "helvetia-card-title"); 
        // Maybe make it a bit larger, but CSS will handle that if we add a class
        
        gtk_box_append(GTK_BOX(main_box), cat_label);

        // FlowBox for tools
        GtkWidget *flowbox = gtk_flow_box_new();
        gtk_widget_set_valign(flowbox, GTK_ALIGN_START);
        gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(flowbox), 2);
        gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flowbox), 5);
        gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flowbox), GTK_SELECTION_NONE);
        gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(flowbox), 20);
        gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(flowbox), 20);
        
        gtk_box_append(GTK_BOX(main_box), flowbox);

        if (sub->tools) {
            for (int j = 0; sub->tools[j].id != NULL; j++) {
                GtkWidget *card = make_tool_card(&sub->tools[j]);
                gtk_flow_box_insert(GTK_FLOW_BOX(flowbox), card, -1);
            }
        }
    }

    return scrolled;
}
