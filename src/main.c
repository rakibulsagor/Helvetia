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

static const HelvetiaTool *startup_tool = NULL;

static void on_activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;
    HelvetiaWindow *win = helvetia_window_new(app);
    gtk_window_present(GTK_WINDOW(win));
    
    if (startup_tool) {
        helvetia_window_open_tool(win, startup_tool);
        startup_tool = NULL;
    }
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

    /* CLI Routing */
    if (argc > 1) {
        if (g_strcmp0(argv[1], "--help") == 0 || g_strcmp0(argv[1], "-h") == 0) {
            g_print("Usage: helvetia [tool-command]\n");
            g_print("If a tool-command is provided, Helvetia will open directly to that tool.\n");
            return 0;
        }
        
        startup_tool = helvetia_tool_registry_find_by_cli(argv[1]);
        
        if (!startup_tool) {
            g_printerr("Unknown tool command: %s\n", argv[1]);
            /* We don't abort, just open normally */
        }
    }

    /* 4. Create and run the GTK4 application */
    GtkApplication *app =
        gtk_application_new(APP_ID, G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);

    /* Remove arguments so GTK doesn't complain about unknown CLI flags */
    int status = g_application_run(G_APPLICATION(app), 1, argv);

    /* 5. Clean up */
    helvetia_plugin_loader_shutdown();
    helvetia_module_registry_shutdown();
    helvetia_tool_registry_cleanup();
    g_object_unref(app);
    return status;
}
