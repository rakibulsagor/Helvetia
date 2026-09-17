#pragma once
#include <gtk/gtk.h>
#include "../core/plugin.h"

/**
 * Automatically generates a view for a module based on its subcategories and tools.
 * Returns a GtkWidget (usually a GtkScrolledWindow) containing the grid.
 */
GtkWidget *helvetia_module_view_new(const HelvetiaModule *module);
