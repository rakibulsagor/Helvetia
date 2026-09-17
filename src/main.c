/* ================================================================
 * Helvetia — main.c
 * Entry point: initialise registry, load plugins, launch GTK4 app.
 * ================================================================ */
#include <gtk/gtk.h>
#include "config.h"
#include "ui/window.h"
#include "core/module_registry.h"
#include "core/plugin_loader.h"
#include "core/tool_registry.h"

static void on_activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;
    HelvetiaWindow *win = helvetia_window_new(app);
    gtk_window_present(GTK_WINDOW(win));
}

int main(int argc, char **argv) {
    /* 1. Initialise the module registry and tool registry */
    helvetia_tool_registry_init();
    helvetia_module_registry_init();

    /* 2. Register all built-in (statically linked) modules */
    helvetia_register_builtin_modules();

    /* 3. Scan the plugin directory for external .so plugins */
    helvetia_plugin_loader_init(PLUGIN_DIR);
    helvetia_plugin_loader_load_all();

    /* 4. Create and run the GTK4 application */
    GtkApplication *app =
        gtk_application_new(APP_ID, G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);

    /* 5. Clean up */
    helvetia_plugin_loader_shutdown();
    helvetia_module_registry_shutdown();
    helvetia_tool_registry_cleanup();
    g_object_unref(app);
    return status;
}
