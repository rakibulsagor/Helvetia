#pragma once

#include <glib.h>

/* Persistent, local-only set of tool IDs saved under XDG_STATE_HOME. */
void     helvetia_favorites_init(void);
void     helvetia_favorites_shutdown(void);
gboolean helvetia_favorites_contains(const char *tool_id);
gboolean helvetia_favorites_toggle(const char *tool_id);
guint    helvetia_favorites_count(void);
