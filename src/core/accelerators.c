#include "accelerators.h"
#include "tool_registry.h"
#include <gtk/gtk.h>

void helvetia_register_accelerators(GtkApplication *app) {
  const char *open_accels[] = {"<Primary>o", NULL};
  const char *save_accels[] = {"<Primary>s", NULL};
  const char *quit_accels[] = {"<Primary>q", NULL};

  gtk_application_set_accels_for_action(app, "app.open", open_accels);
  gtk_application_set_accels_for_action(app, "win.save", save_accels);
  gtk_application_set_accels_for_action(app, "app.quit", quit_accels);
}

void register_tool_command_accels(GtkApplication *app) {
    for (guint i = 0; i < helvetia_tool_registry_count(); i++) {
        const HelvetiaTool *tool = helvetia_tool_registry_get(i);
        if (!tool->commands) continue;

        for (const HelvetiaToolCommand *c = tool->commands; c->id; c++) {
            if (!c->accel) continue;

            /* GTK uses the "::" separator and GVariant text for targets */
            char *action_name = g_strdup_printf(
                "win.tool_action::('%s', '%s')", tool->id, c->id);

            const char *accels[] = { c->accel, NULL };
            gtk_application_set_accels_for_action(app, action_name, accels);

            g_free(action_name);
        }
    }
}
