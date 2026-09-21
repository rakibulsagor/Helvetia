#include "window.h"

#include "../core/module_registry.h"
#include "../core/tool_registry.h"
#include "../core/favorites.h"
#include "../core/tasks/task_queue.h"
#include "module_view.h"
#include "task_strip.h"

struct _HelvetiaWindow {
    AdwApplicationWindow parent_instance;
    GtkWidget *toolbar_view;     /* AdwToolbarView holding the app chrome */
    GtkWidget *outer_stack;    /* "grid" | "tool" */
    GtkWidget *dashboard_box;  /* holds all modules vertically */
    GtkWidget *dashboard_scroll;
    GtkWidget *tool_stack;     /* one child per tool (lazy) */
    GtkWidget *search_entry;
    GtkWidget *search_results; /* GtkListBox shown during search */
    GtkWidget *search_scroll;  /* Scrolled window holding search_results */
    GtkWidget *content_box;    /* holds search or module_stack */
    GtkWidget *favorites_window;
    GtkWidget *favorites_list;

    AdwHeaderBar *tool_header;
    GtkWidget    *tool_actions_box;   /* dynamic container for tool buttons */
    const HelvetiaTool *current_tool;
    GtkWidget *current_tool_view;

    HelvetiaTaskQueue *task_queue;
    HelvetiaTaskStrip *task_strip;
};

G_DEFINE_TYPE(HelvetiaWindow, helvetia_window, ADW_TYPE_APPLICATION_WINDOW)

static void on_save(GSimpleAction *action, GVariant *param, gpointer user_data) {
    (void)action; (void)param; (void)user_data;
    g_print("Save triggered\n");
}

static void populate_tool_header(HelvetiaWindow *self, const HelvetiaTool *tool) {
    if (!self->tool_actions_box) return;

    /* Empty the box */
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(self->tool_actions_box)))
        gtk_box_remove(GTK_BOX(self->tool_actions_box), child);

    if (!tool || !tool->commands) return;

    for (const HelvetiaToolCommand *c = tool->commands; c->id; c++) {
        if (!c->icon_name) continue;   /* skip commands without icons */

        GtkWidget *btn = gtk_button_new_from_icon_name(c->icon_name);
        gtk_widget_add_css_class(btn, "flat");
        if (c->tooltip)
            gtk_widget_set_tooltip_text(btn, c->tooltip);

        gtk_actionable_set_action_name(GTK_ACTIONABLE(btn), "win.tool_action");
        gtk_actionable_set_action_target(GTK_ACTIONABLE(btn), "(ss)", tool->id, c->id);

        gtk_box_append(GTK_BOX(self->tool_actions_box), btn);
    }
}

static void on_tool_action(GSimpleAction *action, GVariant *param, gpointer user_data) {
    HelvetiaWindow *self = HELVETIA_WINDOW(user_data);
    (void)action;

    const char *tool_id    = NULL;
    const char *command_id = NULL;

    g_variant_get(param, "(&s&s)", &tool_id, &command_id);

    const HelvetiaTool *tool = NULL;

    if (tool_id && *tool_id) {
        tool = helvetia_tool_registry_find(tool_id);
    } else {
        tool = self->current_tool;
    }

    if (!tool) {
        g_warning("tool_action: no tool resolved for id '%s'",
                  tool_id ? tool_id : "(current)");
        return;
    }

    if (self->current_tool != tool) {
        helvetia_window_open_tool(self, tool);
    }

    if (!tool->commands) {
        g_warning("tool_action: tool '%s' has no commands", tool->id);
        return;
    }

    const HelvetiaToolCommand *cmd = NULL;
    for (const HelvetiaToolCommand *c = tool->commands; c->id; c++) {
        if (g_strcmp0(c->id, command_id) == 0) {
            cmd = c;
            break;
        }
    }

    if (!cmd || !cmd->activate) {
        g_warning("tool_action: tool '%s' has no command '%s'",
                  tool->id, command_id ? command_id : "(null)");
        return;
    }

    if (!self->current_tool_view) {
        g_warning("tool_action: no view for tool '%s'", tool->id);
        return;
    }

    cmd->activate(self->current_tool_view);
}

static const GActionEntry win_actions[] = {
    { .name = "save", .activate = on_save },
    { .name = "tool_action", .activate = on_tool_action, .parameter_type = "(ss)" },
};

/* ============================================================
 *  Tool navigation — open / close
 * ============================================================ */

/* Navigate back from tool view to grid */
static void on_back_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    HelvetiaWindow *self = user_data;
    
    if (self->current_tool) {
        if (self->current_tool->on_close && self->current_tool_view) {
            self->current_tool->on_close(self->current_tool_view);
        }
        self->current_tool = NULL;
        self->current_tool_view = NULL;
        populate_tool_header(self, NULL);
    }
    
    gtk_stack_set_visible_child_name(GTK_STACK(self->outer_stack), "grid");
    gtk_editable_set_text(GTK_EDITABLE(self->search_entry), "");
}

typedef struct {
    HelvetiaWindow *window;
    const HelvetiaTool *tool;
} FavoriteRowData;

static void favorite_row_data_free(gpointer data, GClosure *closure)
{
    (void)closure;
    g_free(data);
}

static void refresh_favorites(HelvetiaWindow *self);

static void on_favorite_clicked(GtkButton *button, gpointer user_data)
{
    HelvetiaWindow *self = user_data;
    const HelvetiaTool *tool = g_object_get_data(G_OBJECT(button), "tool");
    if (!tool)
        return;

    gboolean added = helvetia_favorites_toggle(tool->id);
    gtk_button_set_icon_name(button, added ? "starred-symbolic" : "non-starred-symbolic");
    gtk_widget_set_tooltip_text(GTK_WIDGET(button),
                                added ? "Remove from favorites" : "Add to favorites");
    refresh_favorites(self);
}

static void on_favorite_row_clicked(GtkButton *button, gpointer user_data)
{
    FavoriteRowData *data = user_data;
    helvetia_window_open_tool(data->window, data->tool);
    if (data->window->favorites_window)
        gtk_window_present(GTK_WINDOW(data->window));
    (void)button;
}

static void on_favorites_window_destroy(GtkWidget *widget, gpointer user_data)
{
    HelvetiaWindow *self = user_data;
    if (self->favorites_window == widget) {
        self->favorites_window = NULL;
        self->favorites_list = NULL;
    }
}

static void refresh_favorites(HelvetiaWindow *self)
{
    if (!self->favorites_list)
        return;

    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(self->favorites_list)) != NULL)
        gtk_list_box_remove(GTK_LIST_BOX(self->favorites_list), child);

    guint count = 0;
    for (guint i = 0; i < helvetia_module_registry_count(); i++) {
        const HelvetiaModule *module = helvetia_module_registry_get(i);
        if (!module || !module->subcategories)
            continue;
        for (guint category = 0; module->subcategories[category].name; category++) {
            const HelvetiaTool *module_tools = module->subcategories[category].tools;
            for (guint j = 0; module_tools && module_tools[j].id; j++) {
                const HelvetiaTool *tool = &module_tools[j];
                if (!helvetia_favorites_contains(tool->id))
                    continue;
                GtkWidget *button = gtk_button_new();
                GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
                GtkWidget *icon = gtk_image_new_from_icon_name(
                    tool->icon_name ? tool->icon_name : "applications-utilities-symbolic");
                GtkWidget *label = gtk_label_new(tool->name);
                gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
                gtk_widget_set_hexpand(label, TRUE);
                gtk_box_append(GTK_BOX(row), icon);
                gtk_box_append(GTK_BOX(row), label);
                gtk_button_set_child(GTK_BUTTON(button), row);
                gtk_widget_set_margin_start(button, 8);
                gtk_widget_set_margin_end(button, 8);
                gtk_widget_set_margin_top(button, 4);
                gtk_widget_set_margin_bottom(button, 4);
                FavoriteRowData *data = g_new(FavoriteRowData, 1);
                data->window = self;
                data->tool = tool;
                g_signal_connect_data(button, "clicked", G_CALLBACK(on_favorite_row_clicked),
                                      data, favorite_row_data_free, 0);
                gtk_list_box_append(GTK_LIST_BOX(self->favorites_list), button);
                count++;
            }
        }
    }
    if (count == 0) {
        GtkWidget *empty = gtk_label_new("No favorite tools yet. Use the star on a tool page to add one.");
        gtk_label_set_wrap(GTK_LABEL(empty), TRUE);
        gtk_widget_set_margin_top(empty, 24);
        gtk_widget_set_margin_bottom(empty, 24);
        gtk_list_box_append(GTK_LIST_BOX(self->favorites_list), empty);
    }
}

static void on_show_favorites(GtkButton *button, gpointer user_data)
{
    HelvetiaWindow *self = user_data;
    if (!self->favorites_window) {
        GtkWidget *window = gtk_application_window_new(
            GTK_APPLICATION(gtk_window_get_application(GTK_WINDOW(self))));
        gtk_window_set_title(GTK_WINDOW(window), "Helvetia Favorites");
        gtk_window_set_default_size(GTK_WINDOW(window), 420, 560);
        gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(self));
        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        GtkWidget *title = gtk_label_new("Favorite Tools");
        gtk_widget_add_css_class(title, "title-2");
        gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
        gtk_widget_set_margin_start(title, 18);
        gtk_widget_set_margin_end(title, 18);
        gtk_widget_set_margin_top(title, 18);
        gtk_widget_set_margin_bottom(title, 12);
        self->favorites_list = gtk_list_box_new();
        GtkWidget *scroll = gtk_scrolled_window_new();
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), self->favorites_list);
        gtk_widget_set_vexpand(scroll, TRUE);
        gtk_box_append(GTK_BOX(box), title);
        gtk_box_append(GTK_BOX(box), scroll);
        gtk_window_set_child(GTK_WINDOW(window), box);
        self->favorites_window = window;
        g_signal_connect(window, "destroy", G_CALLBACK(on_favorites_window_destroy), self);
    }
    refresh_favorites(self);
    gtk_window_present(GTK_WINDOW(self->favorites_window));
    (void)button;
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

        GtkWidget *favorite_btn = gtk_button_new_from_icon_name(
            helvetia_favorites_contains(tool->id) ? "starred-symbolic" : "non-starred-symbolic");
        gtk_widget_add_css_class(favorite_btn, "flat");
        gtk_widget_set_tooltip_text(favorite_btn,
            helvetia_favorites_contains(tool->id) ? "Remove from favorites" : "Add to favorites");
        g_object_set_data(G_OBJECT(favorite_btn), "tool", (gpointer)tool);
        g_signal_connect(favorite_btn, "clicked", G_CALLBACK(on_favorite_clicked), self);
        gtk_box_append(GTK_BOX(topbar), favorite_btn);

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

            GtkWidget *lbl = gtk_label_new("This tool is unavailable in this build");
            gtk_widget_add_css_class(lbl, "helvetia-card-title");

            GtkWidget *sub = gtk_label_new(
                "Its catalog entry is present, but no local backend has been linked yet.");
            gtk_widget_add_css_class(sub, "helvetia-fg-muted");
            gtk_label_set_wrap(GTK_LABEL(sub), TRUE);

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

        g_object_set_data(G_OBJECT(page_box), "tool-view", tool_content);
        gtk_stack_add_named(GTK_STACK(self->tool_stack), page_box, tool->id);
    }

    self->current_tool = tool;
    
    GtkWidget *page_box = gtk_stack_get_child_by_name(GTK_STACK(self->tool_stack), tool->id);
    self->current_tool_view = g_object_get_data(G_OBJECT(page_box), "tool-view");

    gtk_stack_set_visible_child_name(GTK_STACK(self->tool_stack), tool->id);
    gtk_stack_set_visible_child_name(GTK_STACK(self->outer_stack), "tool");
    
    if (self->current_tool->on_open && self->current_tool_view) {
        self->current_tool->on_open(self->current_tool_view);
    }
    
    populate_tool_header(self, self->current_tool);
}

/* ============================================================
 *  Module grid helpers
 * ============================================================ */

static void add_module(HelvetiaWindow *self, const HelvetiaModule *m) {
    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_start(header_box, 24);
    gtk_widget_set_margin_top(header_box, 32);
    gtk_widget_set_margin_bottom(header_box, 8);
    
    if (m->icon_name) {
        GtkWidget *icon = gtk_image_new_from_icon_name(m->icon_name);
        gtk_image_set_pixel_size(GTK_IMAGE(icon), 32);
        gtk_box_append(GTK_BOX(header_box), icon);
    }
    
    GtkWidget *title = gtk_label_new(m->name);
    gtk_widget_add_css_class(title, "helvetia-app-title");
    gtk_box_append(GTK_BOX(header_box), title);
    
    gtk_box_append(GTK_BOX(self->dashboard_box), header_box);

    GtkWidget *view;
    if (m->create_view) {
        view = m->create_view();
        gtk_widget_set_size_request(view, -1, 480);
    }
    else {
        view = helvetia_module_view_new(m, self);
    }

    if (!view)
        view = gtk_label_new("No tools available");

    gtk_box_append(GTK_BOX(self->dashboard_box), view);
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

    /* Ensure we are looking at the grid/search page */
    gtk_stack_set_visible_child_name(GTK_STACK(self->outer_stack), "grid");

    /* Clear old results */
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(self->search_results)) != NULL)
        gtk_list_box_remove(GTK_LIST_BOX(self->search_results), child);

    if (!query || !*query) {
        gtk_widget_set_visible(self->search_scroll, FALSE);
        gtk_widget_set_visible(self->dashboard_scroll, TRUE);
        return;
    }

    GPtrArray *hits = helvetia_tool_registry_search(query, 20);
    if (!hits || hits->len == 0) {
        gtk_widget_set_visible(self->search_scroll, FALSE);
        gtk_widget_set_visible(self->dashboard_scroll, TRUE);
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

    gtk_widget_set_visible(self->search_scroll, TRUE);
    gtk_widget_set_visible(self->dashboard_scroll, FALSE);
}

/* ============================================================
 *  Dark Mode Toggle
 * ============================================================ */

static void on_dark_mode_clicked(GtkButton *btn, gpointer user_data) {
    (void)user_data;
    AdwStyleManager *manager = adw_style_manager_get_default();
    gboolean is_dark = adw_style_manager_get_dark(manager);
    
    if (is_dark) {
        adw_style_manager_set_color_scheme(manager, ADW_COLOR_SCHEME_FORCE_LIGHT);
        gtk_button_set_icon_name(btn, "weather-clear-night-symbolic");
    } else {
        adw_style_manager_set_color_scheme(manager, ADW_COLOR_SCHEME_FORCE_DARK);
        gtk_button_set_icon_name(btn, "weather-clear-symbolic");
    }
}

/* ============================================================
 *  GObject boilerplate
 * ============================================================ */

static void helvetia_window_class_init(HelvetiaWindowClass *klass) {
    (void)klass;
}

static void helvetia_window_init(HelvetiaWindow *self) {
    g_action_map_add_action_entries(G_ACTION_MAP(self), win_actions, G_N_ELEMENTS(win_actions), self);

    gtk_window_set_default_size(GTK_WINDOW(self), 1280, 800);
    gtk_window_set_title(GTK_WINDOW(self), "Helvetia — All-in-One Toolkit");

    /* CSS */
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_path(css, "src/ui/theme.css");
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(), GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);

    /* === Main content area === */
    self->toolbar_view = adw_toolbar_view_new();
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(self->toolbar_view), main_box);

    /* App Header / Search bar */
    GtkWidget *header_bar = adw_header_bar_new();
    self->tool_header = ADW_HEADER_BAR(header_bar);
    
    GtkWidget *app_icon = gtk_image_new_from_icon_name("applications-utilities");
    gtk_image_set_pixel_size(GTK_IMAGE(app_icon), 24);
    GtkWidget *app_name = gtk_label_new("Helvetia");
    gtk_widget_add_css_class(app_name, "helvetia-app-title");
    
    GtkWidget *start_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_append(GTK_BOX(start_box), app_icon);
    gtk_box_append(GTK_BOX(start_box), app_name);
    
    self->search_entry = gtk_search_entry_new();
    gtk_widget_set_hexpand(self->search_entry, TRUE);
    g_signal_connect(self->search_entry, "search-changed",
                     G_CALLBACK(on_search_changed), self);

    adw_header_bar_pack_start(ADW_HEADER_BAR(header_bar), start_box);
    adw_header_bar_set_title_widget(ADW_HEADER_BAR(header_bar), self->search_entry);

    GtkWidget *favorites_btn = gtk_button_new_from_icon_name("starred-symbolic");
    gtk_widget_add_css_class(favorites_btn, "flat");
    gtk_widget_set_tooltip_text(favorites_btn, "Show favorite tools");
    g_signal_connect(favorites_btn, "clicked", G_CALLBACK(on_show_favorites), self);
    
    GtkWidget *stat_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_halign(stat_bar, GTK_ALIGN_END);
    gtk_widget_set_valign(stat_bar, GTK_ALIGN_CENTER);
    GtkWidget *stat_icon = gtk_image_new_from_icon_name("system-run-symbolic");
    GtkWidget *stat_lbl = gtk_label_new("150+ Tools Available");
    gtk_widget_add_css_class(stat_lbl, "helvetia-fg-muted");
    gtk_box_append(GTK_BOX(stat_bar), stat_icon);
    gtk_box_append(GTK_BOX(stat_bar), stat_lbl);
    
    GtkWidget *theme_btn = gtk_button_new_from_icon_name(
        adw_style_manager_get_dark(adw_style_manager_get_default()) ? "weather-clear-symbolic" : "weather-clear-night-symbolic");
    gtk_widget_add_css_class(theme_btn, "flat");
    gtk_widget_set_tooltip_text(theme_btn, "Toggle Dark Mode");
    g_signal_connect(theme_btn, "clicked", G_CALLBACK(on_dark_mode_clicked), NULL);
    
    adw_header_bar_pack_end(ADW_HEADER_BAR(header_bar), theme_btn);
    adw_header_bar_pack_end(ADW_HEADER_BAR(header_bar), favorites_btn);
    adw_header_bar_pack_end(ADW_HEADER_BAR(header_bar), stat_bar);
    
    self->tool_actions_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_add_css_class(self->tool_actions_box, "linked");
    adw_header_bar_pack_end(ADW_HEADER_BAR(header_bar), self->tool_actions_box);
    
    adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(self->toolbar_view), header_bar);

    /* outer_stack: "grid" page holds dashboard+search, "tool" page holds tool_stack */
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

    self->dashboard_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    self->dashboard_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(self->dashboard_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(self->dashboard_scroll), self->dashboard_box);
    gtk_widget_set_hexpand(self->dashboard_scroll, TRUE);
    gtk_widget_set_vexpand(self->dashboard_scroll, TRUE);

    /* Search results overlay */
    self->search_results = gtk_list_box_new();
    gtk_widget_add_css_class(self->search_results, "helvetia-nav");
    gtk_widget_set_vexpand(self->search_results, TRUE);
    g_signal_connect(self->search_results, "row-activated",
                     G_CALLBACK(on_search_row_activated), NULL);

    self->search_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(self->search_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(self->search_scroll), self->search_results);
    gtk_widget_set_vexpand(self->search_scroll, TRUE);
    gtk_widget_set_visible(self->search_scroll, FALSE);

    gtk_box_append(GTK_BOX(grid_page), self->dashboard_scroll);
    gtk_box_append(GTK_BOX(grid_page), self->search_scroll);

    /* Tool page */
    self->tool_stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(self->tool_stack),
                                  GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    gtk_stack_set_transition_duration(GTK_STACK(self->tool_stack), 200);
    gtk_widget_set_hexpand(self->tool_stack, TRUE);
    gtk_widget_set_vexpand(self->tool_stack, TRUE);

    gtk_stack_add_named(GTK_STACK(self->outer_stack), grid_page, "grid");
    gtk_stack_add_named(GTK_STACK(self->outer_stack), self->tool_stack, "tool");

    gtk_box_append(GTK_BOX(main_box), self->outer_stack);
    adw_application_window_set_content(ADW_APPLICATION_WINDOW(self), self->toolbar_view);

    /* Task queue + strip */
    self->task_queue = helvetia_task_queue_get_default();
    self->task_strip = helvetia_task_strip_new(self->task_queue);
    adw_toolbar_view_add_bottom_bar(ADW_TOOLBAR_VIEW(self->toolbar_view),
                                    GTK_WIDGET(self->task_strip));

    /* Register modules */
    guint count = helvetia_module_registry_count();
    for (guint i = 0; i < count; i++) {
        const HelvetiaModule *m = helvetia_module_registry_get(i);
        add_module(self, m);
    }
}

HelvetiaWindow *helvetia_window_new(AdwApplication *app) {
    return g_object_new(HELVETIA_TYPE_WINDOW, "application", app, NULL);
}
