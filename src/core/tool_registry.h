#pragma once
#include <glib.h>
#include "plugin.h"

void helvetia_tool_registry_init    (void);
void helvetia_tool_registry_cleanup (void);
void helvetia_tool_registry_index_module(const HelvetiaModule *module);

/* Returns a GPtrArray of (const HelvetiaTool *); max_results=0 means unlimited */
GPtrArray          *helvetia_tool_registry_search     (const char *query, guint max_results);
const HelvetiaTool *helvetia_tool_registry_find_by_cli(const char *cli_command);
