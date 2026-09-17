#include "window.h"
#include "../core/module_registry.h"
#include "../core/tool_registry.h"
#include "module_view.h"

/* ============================================================
 *  Private instance struct
 * ============================================================ */
struct _HelvetiaWindow {
    GtkApplicationWindow parent_instance;
    GtkWidget *stack;    /* GtkStack — one child per module */
    GtkWidget *sidebar;  /* GtkListBox — navigation         */
};

G_DEFINE_TYPE(HelvetiaWindow, helvetia_window, GTK_TYPE_APPLICATION_WINDOW)

/* ============================================================
 *  Helpers
 * ============================================================ */

/** Add a module's view as a named page in the stack and a row in sidebar. */
static void add_module(HelvetiaWindow *self, const HelvetiaModule *m) {
    /* --- Build the page content --- */
    GtkWidget *view = NULL;
    if (m->create_view) {
        view = m->create_view();
    } else {
        view = helvetia_module_view_new(m);
    }
    
    if (!view) {
        /* Fallback if module has no UI or fails to build */
        view = gtk_label_new("No tools available");
    }

    gtk_stack_add_named(GTK_STACK(self->stack), view, m->id);

    /* --- Build the sidebar row --- */
    GtkWidget *row = gtk_list_box_row_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_start(box, 12);
    gtk_widget_set_margin_end(box, 12);
    gtk_widget_set_margin_top(box, 8);
    gtk_widget_set_margin_bottom(box, 8);

    const char *icon = m->icon_name ? m->icon_name : "application-x-executable";
    GtkWidget *img   = gtk_image_new_from_icon_name(icon);
    gtk_image_set_pixel_size(GTK_IMAGE(img), 18);

    GtkWidget *lbl = gtk_label_new(m->name);
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);
    gtk_widget_set_hexpand(lbl, TRUE);

    gtk_box_append(GTK_BOX(box), img);
    gtk_box_append(GTK_BOX(box), lbl);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);

    /* Store module id so we can switch to it on selection */
    g_object_set_data_full(G_OBJECT(row), "module-id",
                           g_strdup(m->id), g_free);

    if (m->description)
        gtk_widget_set_tooltip_text(row, m->description);

    gtk_list_box_append(GTK_LIST_BOX(self->sidebar), row);
}

/* Sidebar selection → switch the visible stack page */
static void on_row_activated(GtkListBox     *box,
                              GtkListBoxRow  *row,
                              gpointer        user_data)
{
    (void)box;
    HelvetiaWindow *self = user_data;
    const char *id = g_object_get_data(G_OBJECT(row), "module-id");
    if (id) gtk_stack_set_visible_child_name(GTK_STACK(self->stack), id);
}

/* ============================================================
 *  GObject boilerplate
 * ============================================================ */

static void helvetia_window_class_init(HelvetiaWindowClass *klass) {
    (void)klass; /* nothing to override yet */
}

static void helvetia_window_init(HelvetiaWindow *self) {
    gtk_window_set_default_size(GTK_WINDOW(self), 1200, 740);
    gtk_window_set_title(GTK_WINDOW(self), "Helvetia — All-in-One Toolkit");

    /* Apply CSS theme */
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_path(css, "src/ui/theme.css");
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);

    /* ---- Root layout: sidebar | content (paned) ---- */
    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_wide_handle(GTK_PANED(paned), FALSE);

    /* Sidebar */
    GtkWidget *sidebar_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(sidebar_box, "helvetia-sidebar");

    /* App header inside sidebar */
    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(header_box, "helvetia-header");
    gtk_widget_set_margin_start(header_box, 16);
    gtk_widget_set_margin_end(header_box, 16);
    gtk_widget_set_margin_top(header_box, 20);
    gtk_widget_set_margin_bottom(header_box, 20);

    GtkWidget *app_icon = gtk_image_new_from_icon_name("applications-utilities");
    gtk_image_set_pixel_size(GTK_IMAGE(app_icon), 28);
    GtkWidget *app_name = gtk_label_new("Helvetia");
    gtk_widget_add_css_class(app_name, "helvetia-app-title");

    gtk_box_append(GTK_BOX(header_box), app_icon);
    gtk_box_append(GTK_BOX(header_box), app_name);

    /* Separator */
    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);

    self->sidebar = gtk_list_box_new();
    gtk_widget_add_css_class(self->sidebar, "helvetia-nav");
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(self->sidebar),
                                    GTK_SELECTION_SINGLE);
    g_signal_connect(self->sidebar, "row-activated",
                     G_CALLBACK(on_row_activated), self);

    GtkWidget *sidebar_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sidebar_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sidebar_scroll),
                                  self->sidebar);
    gtk_widget_set_vexpand(sidebar_scroll, TRUE);

    gtk_box_append(GTK_BOX(sidebar_box), header_box);
    gtk_box_append(GTK_BOX(sidebar_box), sep);
    gtk_box_append(GTK_BOX(sidebar_box), sidebar_scroll);

    /* Content stack wrapper to hold Search bar and Stack */
    GtkWidget *content_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    
    GtkWidget *search_bar_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_margin_start(search_bar_box, 24);
    gtk_widget_set_margin_end(search_bar_box, 24);
    gtk_widget_set_margin_top(search_bar_box, 16);
    gtk_widget_set_margin_bottom(search_bar_box, 0);
    
    GtkWidget *search_entry = gtk_search_entry_new();
    gtk_widget_set_hexpand(search_entry, TRUE);
    gtk_box_append(GTK_BOX(search_bar_box), search_entry);
    
    gtk_box_append(GTK_BOX(content_box), search_bar_box);

    /* Content stack */
    self->stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(self->stack),
                                  GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    gtk_stack_set_transition_duration(GTK_STACK(self->stack), 200);
    gtk_widget_set_hexpand(self->stack, TRUE);
    gtk_widget_set_vexpand(self->stack, TRUE);

    gtk_box_append(GTK_BOX(content_box), self->stack);

    gtk_paned_set_start_child(GTK_PANED(paned), sidebar_box);
    gtk_paned_set_end_child  (GTK_PANED(paned), content_box);
    gtk_paned_set_position   (GTK_PANED(paned), 260);
    gtk_paned_set_resize_start_child(GTK_PANED(paned), FALSE);

    gtk_window_set_child(GTK_WINDOW(self), paned);

    /* Register all modules into the window and tool registry */
    guint count = helvetia_module_registry_count();
    for (guint i = 0; i < count; i++) {
        const HelvetiaModule *m = helvetia_module_registry_get(i);
        helvetia_tool_registry_index_module(m);
        add_module(self, m);
    }

    /* Select the first row by default */
    GtkListBoxRow *first = gtk_list_box_get_row_at_index(
                               GTK_LIST_BOX(self->sidebar), 0);
    if (first) {
        gtk_list_box_select_row(GTK_LIST_BOX(self->sidebar), first);
        on_row_activated(GTK_LIST_BOX(self->sidebar), first, self);
    }
}

HelvetiaWindow *helvetia_window_new(GtkApplication *app) {
    return g_object_new(HELVETIA_TYPE_WINDOW, "application", app, NULL);
}
