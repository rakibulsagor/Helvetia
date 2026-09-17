#pragma once
#include <glib.h>
#include "plugin.h"

void helvetia_tool_registry_init(void);
void helvetia_tool_registry_cleanup(void);

/* Called by the module registry when a module is added */
void helvetia_tool_registry_index_module(const HelvetiaModule *module);

/* Returns a GPtrArray of (const HelvetiaTool *) matching the search query */
GPtrArray *helvetia_tool_registry_search(const char *query);

/* Returns the tool that matches the exact CLI command name, or NULL */
const HelvetiaTool *helvetia_tool_registry_find_by_cli(const char *cli_command);
