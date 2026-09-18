#include "module_view.h"
#include "window.h"

typedef struct { HelvetiaWindow *win; const HelvetiaTool *tool; } CardCtx;

static void on_card_clicked(GtkGestureClick *g, int n_press, double x, double y, gpointer data) {
    (void)g; (void)n_press; (void)x; (void)y;
    CardCtx *ctx = data;
    helvetia_window_open_tool(ctx->win, ctx->tool);
}

static GtkWidget *make_tool_card(const HelvetiaTool *tool, HelvetiaWindow *win) {
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(card, "helvetia-card");

    /* Make card clickable */
    GtkGesture *click = gtk_gesture_click_new();
    CardCtx *ctx = g_new(CardCtx, 1);
    ctx->win = win;
    ctx->tool = tool;
    g_signal_connect(click, "pressed", G_CALLBACK(on_card_clicked), ctx);
    /* clean up ctx when card widget is destroyed */
    g_signal_connect_swapped(card, "destroy", G_CALLBACK(g_free), ctx);
    gtk_widget_add_controller(card, GTK_EVENT_CONTROLLER(click));

    /* Hover cursor */
    gtk_widget_set_cursor_from_name(card, "pointer");

    /* Header: icon + title */
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

    if (tool->icon_name) {
        GtkWidget *icon = gtk_image_new_from_icon_name(tool->icon_name);
        gtk_image_set_pixel_size(GTK_IMAGE(icon), 16);
        gtk_box_append(GTK_BOX(header), icon);
    }

    GtkWidget *title = gtk_label_new(tool->name);
    gtk_widget_add_css_class(title, "helvetia-card-title");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_widget_set_hexpand(title, TRUE);
    gtk_label_set_ellipsize(GTK_LABEL(title), PANGO_ELLIPSIZE_END);
    gtk_box_append(GTK_BOX(header), title);

    gtk_box_append(GTK_BOX(card), header);

    if (tool->description && *tool->description) {
        GtkWidget *sub = gtk_label_new(tool->description);
        gtk_widget_add_css_class(sub, "helvetia-card-subtitle");
        gtk_widget_set_halign(sub, GTK_ALIGN_START);
        gtk_label_set_ellipsize(GTK_LABEL(sub), PANGO_ELLIPSIZE_END);
        gtk_label_set_lines(GTK_LABEL(sub), 2);
        gtk_label_set_wrap(GTK_LABEL(sub), TRUE);
        gtk_box_append(GTK_BOX(card), sub);
    }

    /* Badge if functional */
    if (tool->create_view) {
        GtkWidget *badge = gtk_label_new("● Ready");
        gtk_widget_add_css_class(badge, "helvetia-badge-ready");
        gtk_widget_set_halign(badge, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(card), badge);
    }

    return card;
}

GtkWidget *helvetia_module_view_new(const HelvetiaModule *module, HelvetiaWindow *win) {
    if (!module || !module->subcategories) return NULL;

    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 24);
    gtk_widget_set_margin_start(main_box, 24);
    gtk_widget_set_margin_end(main_box, 24);
    gtk_widget_set_margin_top(main_box, 20);
    gtk_widget_set_margin_bottom(main_box, 24);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), main_box);

    for (int i = 0; module->subcategories[i].name != NULL; i++) {
        const HelvetiaSubcategory *sub = &module->subcategories[i];

        GtkWidget *cat_label = gtk_label_new(sub->name);
        gtk_widget_set_halign(cat_label, GTK_ALIGN_START);
        gtk_widget_add_css_class(cat_label, "helvetia-section-header");
        gtk_box_append(GTK_BOX(main_box), cat_label);

        GtkWidget *flowbox = gtk_flow_box_new();
        gtk_widget_set_valign(flowbox, GTK_ALIGN_START);
        gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(flowbox), 2);
        gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flowbox), 5);
        gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flowbox), GTK_SELECTION_NONE);
        gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(flowbox), 12);
        gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(flowbox), 12);
        gtk_box_append(GTK_BOX(main_box), flowbox);

        if (sub->tools) {
            for (int j = 0; sub->tools[j].id != NULL; j++) {
                GtkWidget *card = make_tool_card(&sub->tools[j], win);
                gtk_flow_box_insert(GTK_FLOW_BOX(flowbox), card, -1);
            }
        }
    }

    return scrolled;
}
