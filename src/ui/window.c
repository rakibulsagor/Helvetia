#include "window.h"
#include "widgets.h"
#include "../core/module_registry.h"
#include "../core/tool_registry.h"
#include "module_view.h"

struct _HelvetiaWindow {
    GtkApplicationWindow parent_instance;
    GtkWidget *outer_stack;    /* "grid" | "tool" */
    GtkWidget *module_stack;   /* one child per module */
    GtkWidget *tool_stack;     /* one child per tool (lazy) */
    GtkWidget *sidebar;
    GtkWidget *search_entry;
    GtkWidget *search_results; /* GtkListBox shown during search */
    GtkWidget *content_box;    /* holds search or module_stack */
};

G_DEFINE_TYPE(HelvetiaWindow, helvetia_window, GTK_TYPE_APPLICATION_WINDOW)

/* ============================================================
 *  Tool navigation — open / close
 * ============================================================ */

/* Navigate back from tool view to grid */
static void on_back_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    HelvetiaWindow *self = user_data;
    gtk_stack_set_visible_child_name(GTK_STACK(self->outer_stack), "grid");
}

/* Open a tool's dedicated full-panel view */
void helvetia_window_open_tool(HelvetiaWindow *self, const HelvetiaTool *tool) {
    if (!tool) return;

    /* Check if already built */
    GtkWidget *existing = gtk_stack_get_child_by_name(GTK_STACK(self->tool_stack), tool->id);
    if (!existing) {
        /* Build the tool page */
        GtkWidget *page_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

        /* --- Top bar with back button + title --- */
        GtkWidget *topbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_add_css_class(topbar, "helvetia-topbar");
        gtk_widget_set_margin_start(topbar, 16);
        gtk_widget_set_margin_end(topbar, 16);
        gtk_widget_set_margin_top(topbar, 12);
        gtk_widget_set_margin_bottom(topbar, 12);

        GtkWidget *back_btn = gtk_button_new_from_icon_name("go-previous-symbolic");
        gtk_widget_add_css_class(back_btn, "flat");
        gtk_widget_set_tooltip_text(back_btn, "Back to tools");
        g_signal_connect(back_btn, "clicked", G_CALLBACK(on_back_clicked), self);

        GtkWidget *icon = gtk_image_new_from_icon_name(
            tool->icon_name ? tool->icon_name : "applications-utilities-symbolic");
        gtk_image_set_pixel_size(GTK_IMAGE(icon), 20);

        GtkWidget *title_lbl = gtk_label_new(tool->name);
        gtk_widget_add_css_class(title_lbl, "helvetia-app-title");
        gtk_widget_set_halign(title_lbl, GTK_ALIGN_START);
        gtk_widget_set_hexpand(title_lbl, TRUE);

        gtk_box_append(GTK_BOX(topbar), back_btn);
        gtk_box_append(GTK_BOX(topbar), icon);
        gtk_box_append(GTK_BOX(topbar), title_lbl);

        if (tool->description && *tool->description) {
            GtkWidget *desc = gtk_label_new(tool->description);
            gtk_widget_add_css_class(desc, "helvetia-fg-muted");
            gtk_label_set_xalign(GTK_LABEL(desc), 0.0f);
            gtk_widget_set_margin_start(desc, 16);
            gtk_widget_set_margin_bottom(desc, 4);
            gtk_box_append(GTK_BOX(page_box), topbar);
            gtk_box_append(GTK_BOX(page_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
            gtk_box_append(GTK_BOX(page_box), desc);
        } else {
            gtk_box_append(GTK_BOX(page_box), topbar);
            gtk_box_append(GTK_BOX(page_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
        }

        /* --- Tool content --- */
        GtkWidget *tool_content;
        if (tool->create_view) {
            tool_content = tool->create_view();
        } else {
            /* Coming soon placeholder */
            GtkWidget *center = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
            gtk_widget_set_valign(center, GTK_ALIGN_CENTER);
            gtk_widget_set_halign(center, GTK_ALIGN_CENTER);
            gtk_widget_set_vexpand(center, TRUE);

            GtkWidget *img = gtk_image_new_from_icon_name("dialog-information-symbolic");
            gtk_image_set_pixel_size(GTK_IMAGE(img), 48);
            gtk_widget_add_css_class(img, "helvetia-fg-muted");

            GtkWidget *lbl = gtk_label_new("This tool is coming soon!");
            gtk_widget_add_css_class(lbl, "helvetia-card-title");

            GtkWidget *sub = gtk_label_new(
                "The underlying library will be implemented in the next phase.");
            gtk_widget_add_css_class(sub, "helvetia-fg-muted");

            gtk_box_append(GTK_BOX(center), img);
            gtk_box_append(GTK_BOX(center), lbl);
            gtk_box_append(GTK_BOX(center), sub);
            tool_content = center;
        }

        GtkWidget *scrolled = gtk_scrolled_window_new();
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_widget_set_vexpand(scrolled, TRUE);

        GtkWidget *content_pad = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_set_margin_start(content_pad, 24);
        gtk_widget_set_margin_end(content_pad, 24);
        gtk_widget_set_margin_top(content_pad, 16);
        gtk_widget_set_margin_bottom(content_pad, 24);
        gtk_box_append(GTK_BOX(content_pad), tool_content);
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), content_pad);
        gtk_box_append(GTK_BOX(page_box), scrolled);

        gtk_stack_add_named(GTK_STACK(self->tool_stack), page_box, tool->id);
    }

    gtk_stack_set_visible_child_name(GTK_STACK(self->tool_stack), tool->id);
    gtk_stack_set_visible_child_name(GTK_STACK(self->outer_stack), "tool");
}

/* ============================================================
 *  Module grid helpers
 * ============================================================ */

static void add_module(HelvetiaWindow *self, const HelvetiaModule *m) {
    GtkWidget *view;
    if (m->create_view)
        view = m->create_view();
    else
        view = helvetia_module_view_new(m, self);

    if (!view)
        view = gtk_label_new("No tools available");

    gtk_stack_add_named(GTK_STACK(self->module_stack), view, m->id);

    /* Sidebar row */
    GtkWidget *row = gtk_list_box_row_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_start(box, 12);
    gtk_widget_set_margin_end(box, 12);
    gtk_widget_set_margin_top(box, 8);
    gtk_widget_set_margin_bottom(box, 8);

    const char *icon_name = m->icon_name ? m->icon_name : "application-x-executable";
    GtkWidget *img = gtk_image_new_from_icon_name(icon_name);
    gtk_image_set_pixel_size(GTK_IMAGE(img), 18);

    GtkWidget *lbl = gtk_label_new(m->name);
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);
    gtk_widget_set_hexpand(lbl, TRUE);

    gtk_box_append(GTK_BOX(box), img);
    gtk_box_append(GTK_BOX(box), lbl);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
    g_object_set_data_full(G_OBJECT(row), "module-id", g_strdup(m->id), g_free);
    if (m->description)
        gtk_widget_set_tooltip_text(row, m->description);
    gtk_list_box_append(GTK_LIST_BOX(self->sidebar), row);
}

static void on_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer user_data) {
    (void)box;
    HelvetiaWindow *self = user_data;
    const char *id = g_object_get_data(G_OBJECT(row), "module-id");
    if (id) {
        gtk_stack_set_visible_child_name(GTK_STACK(self->module_stack), id);
        gtk_stack_set_visible_child_name(GTK_STACK(self->outer_stack), "grid");
    }
}

/* ============================================================
 *  Global Search
 * ============================================================ */

typedef struct { HelvetiaWindow *win; const HelvetiaTool *tool; } SearchRowData;

static void on_search_row_activated(GtkListBox *lb, GtkListBoxRow *row, gpointer ud) {
    (void)lb; (void)ud;
    SearchRowData *d = g_object_get_data(G_OBJECT(row), "search-data");
    if (d) helvetia_window_open_tool(d->win, d->tool);
}

static void on_search_changed(GtkSearchEntry *entry, gpointer user_data) {
    HelvetiaWindow *self = user_data;
    const char *query = gtk_editable_get_text(GTK_EDITABLE(entry));

    /* Clear old results */
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(self->search_results)) != NULL)
        gtk_list_box_remove(GTK_LIST_BOX(self->search_results), child);

    if (!query || !*query) {
        gtk_widget_set_visible(self->search_results, FALSE);
        gtk_widget_set_visible(self->module_stack, TRUE);
        return;
    }

    GPtrArray *hits = helvetia_tool_registry_search(query, 20);
    if (!hits || hits->len == 0) {
        gtk_widget_set_visible(self->search_results, FALSE);
        gtk_widget_set_visible(self->module_stack, TRUE);
        if (hits) g_ptr_array_free(hits, TRUE);
        return;
    }

    for (guint i = 0; i < hits->len; i++) {
        const HelvetiaTool *tool = hits->pdata[i];
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_set_margin_start(box, 12);
        gtk_widget_set_margin_end(box, 12);
        gtk_widget_set_margin_top(box, 8);
        gtk_widget_set_margin_bottom(box, 8);

        GtkWidget *img = gtk_image_new_from_icon_name(
            tool->icon_name ? tool->icon_name : "applications-utilities-symbolic");
        gtk_image_set_pixel_size(GTK_IMAGE(img), 16);

        GtkWidget *lbl = gtk_label_new(tool->name);
        gtk_widget_set_halign(lbl, GTK_ALIGN_START);
        gtk_widget_set_hexpand(lbl, TRUE);

        gtk_box_append(GTK_BOX(box), img);
        gtk_box_append(GTK_BOX(box), lbl);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);

        SearchRowData *d = g_new(SearchRowData, 1);
        d->win = self;
        d->tool = tool;
        g_object_set_data_full(G_OBJECT(row), "search-data", d, g_free);

        gtk_list_box_append(GTK_LIST_BOX(self->search_results), row);
    }
    g_ptr_array_free(hits, TRUE);

    gtk_widget_set_visible(self->search_results, TRUE);
    gtk_widget_set_visible(self->module_stack, FALSE);
}

/* ============================================================
 *  GObject boilerplate
 * ============================================================ */

static void helvetia_window_class_init(HelvetiaWindowClass *klass) {
    (void)klass;
}

static void helvetia_window_init(HelvetiaWindow *self) {
    gtk_window_set_default_size(GTK_WINDOW(self), 1280, 800);
    gtk_window_set_title(GTK_WINDOW(self), "Helvetia — All-in-One Toolkit");

    /* CSS */
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_path(css, "src/ui/theme.css");
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(), GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);

    /* Root paned: sidebar | outer_stack */
    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_wide_handle(GTK_PANED(paned), FALSE);

    /* === Sidebar === */
    GtkWidget *sidebar_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(sidebar_box, "helvetia-sidebar");

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

    self->sidebar = gtk_list_box_new();
    gtk_widget_add_css_class(self->sidebar, "helvetia-nav");
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(self->sidebar), GTK_SELECTION_SINGLE);
    g_signal_connect(self->sidebar, "row-activated", G_CALLBACK(on_row_activated), self);

    GtkWidget *sidebar_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sidebar_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sidebar_scroll), self->sidebar);
    gtk_widget_set_vexpand(sidebar_scroll, TRUE);

    gtk_box_append(GTK_BOX(sidebar_box), header_box);
    gtk_box_append(GTK_BOX(sidebar_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(sidebar_box), sidebar_scroll);

    /* === Right content area === */
    GtkWidget *right_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    /* Search bar */
    GtkWidget *search_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_margin_start(search_bar, 24);
    gtk_widget_set_margin_end(search_bar, 24);
    gtk_widget_set_margin_top(search_bar, 14);
    gtk_widget_set_margin_bottom(search_bar, 8);

    self->search_entry = gtk_search_entry_new();
    gtk_widget_set_hexpand(self->search_entry, TRUE);
    g_signal_connect(self->search_entry, "search-changed",
                     G_CALLBACK(on_search_changed), self);
    gtk_box_append(GTK_BOX(search_bar), self->search_entry);
    gtk_box_append(GTK_BOX(right_box), search_bar);
    gtk_box_append(GTK_BOX(right_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

    /* outer_stack: "grid" page holds module_stack+search, "tool" page holds tool_stack */
    self->outer_stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(self->outer_stack),
                                  GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    gtk_stack_set_transition_duration(GTK_STACK(self->outer_stack), 200);
    gtk_widget_set_hexpand(self->outer_stack, TRUE);
    gtk_widget_set_vexpand(self->outer_stack, TRUE);

    /* Grid page */
    GtkWidget *grid_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(grid_page, TRUE);
    gtk_widget_set_vexpand(grid_page, TRUE);

    self->module_stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(self->module_stack),
                                  GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration(GTK_STACK(self->module_stack), 150);
    gtk_widget_set_hexpand(self->module_stack, TRUE);
    gtk_widget_set_vexpand(self->module_stack, TRUE);

    /* Search results overlay */
    self->search_results = gtk_list_box_new();
    gtk_widget_add_css_class(self->search_results, "helvetia-nav");
    gtk_widget_set_vexpand(self->search_results, TRUE);
    gtk_widget_set_visible(self->search_results, FALSE);
    g_signal_connect(self->search_results, "row-activated",
                     G_CALLBACK(on_search_row_activated), NULL);

    GtkWidget *sr_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sr_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sr_scroll), self->search_results);
    gtk_widget_set_vexpand(sr_scroll, TRUE);

    gtk_box_append(GTK_BOX(grid_page), self->module_stack);
    gtk_box_append(GTK_BOX(grid_page), sr_scroll);

    /* Tool page */
    self->tool_stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(self->tool_stack),
                                  GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    gtk_stack_set_transition_duration(GTK_STACK(self->tool_stack), 200);
    gtk_widget_set_hexpand(self->tool_stack, TRUE);
    gtk_widget_set_vexpand(self->tool_stack, TRUE);

    gtk_stack_add_named(GTK_STACK(self->outer_stack), grid_page, "grid");
    gtk_stack_add_named(GTK_STACK(self->outer_stack), self->tool_stack, "tool");

    gtk_box_append(GTK_BOX(right_box), self->outer_stack);

    gtk_paned_set_start_child(GTK_PANED(paned), sidebar_box);
    gtk_paned_set_end_child  (GTK_PANED(paned), right_box);
    gtk_paned_set_position   (GTK_PANED(paned), 260);
    gtk_paned_set_resize_start_child(GTK_PANED(paned), FALSE);
    gtk_window_set_child(GTK_WINDOW(self), paned);

    /* Register modules */
    guint count = helvetia_module_registry_count();
    for (guint i = 0; i < count; i++) {
        const HelvetiaModule *m = helvetia_module_registry_get(i);
        helvetia_tool_registry_index_module(m);
        add_module(self, m);
    }

    /* Select first row */
    GtkListBoxRow *first = gtk_list_box_get_row_at_index(GTK_LIST_BOX(self->sidebar), 0);
    if (first) {
        gtk_list_box_select_row(GTK_LIST_BOX(self->sidebar), first);
        on_row_activated(GTK_LIST_BOX(self->sidebar), first, self);
    }
}

HelvetiaWindow *helvetia_window_new(GtkApplication *app) {
    return g_object_new(HELVETIA_TYPE_WINDOW, "application", app, NULL);
}
