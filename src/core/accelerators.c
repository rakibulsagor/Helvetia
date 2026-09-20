#include "accelerators.h"
#include <gtk/gtk.h>

void helvetia_register_accelerators(GtkApplication *app) {
  const char *open_accels[] = {"<Primary>o", NULL};
  const char *save_accels[] = {"<Primary>s", NULL};
  const char *quit_accels[] = {"<Primary>q", NULL};

  gtk_application_set_accels_for_action(app, "app.open", open_accels);
  gtk_application_set_accels_for_action(app, "win.save", save_accels);
  gtk_application_set_accels_for_action(app, "app.quit", quit_accels);
}
