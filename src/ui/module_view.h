#pragma once
#include <gtk/gtk.h>
#include "../core/plugin.h"

/* Forward declare to avoid circular headers */
typedef struct _HelvetiaWindow HelvetiaWindow;

GtkWidget *helvetia_module_view_new(const HelvetiaModule *module, HelvetiaWindow *win);
