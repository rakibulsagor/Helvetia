#pragma once
#include <gtk/gtk.h>
#include "../core/plugin.h"

#define HELVETIA_TYPE_WINDOW (helvetia_window_get_type())
G_DECLARE_FINAL_TYPE(HelvetiaWindow, helvetia_window, HELVETIA, WINDOW, GtkApplicationWindow)

HelvetiaWindow *helvetia_window_new      (GtkApplication *app);
void            helvetia_window_open_tool(HelvetiaWindow *self, const HelvetiaTool *tool);
