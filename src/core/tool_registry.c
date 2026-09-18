#include "tool_registry.h"
#include <string.h>

static GPtrArray *all_tools = NULL;

void helvetia_tool_registry_init(void) {
    if (!all_tools) {
        all_tools = g_ptr_array_new();
    }
}

void helvetia_tool_registry_cleanup(void) {
    if (all_tools) {
        g_ptr_array_unref(all_tools);
        all_tools = NULL;
    }
}

void helvetia_tool_registry_index_module(const HelvetiaModule *module) {
    if (!all_tools) return;
    if (!module || !module->subcategories) return;
    
    for (int i = 0; module->subcategories[i].name != NULL; i++) {
        const HelvetiaSubcategory *sub = &module->subcategories[i];
        if (!sub->tools) continue;
        
        for (int j = 0; sub->tools[j].id != NULL; j++) {
            g_ptr_array_add(all_tools, (gpointer)&sub->tools[j]);
        }
    }
}

/* Simple case-insensitive substring search across name, desc, and keywords */
GPtrArray *helvetia_tool_registry_search(const char *query, guint max_results) {
    GPtrArray *results = g_ptr_array_new();
    if (!all_tools || !query || !*query) return results;
    if (max_results == 0) max_results = G_MAXUINT;
    
    char *lower_query = g_utf8_strdown(query, -1);
    
    for (guint i = 0; i < all_tools->len; i++) {
        const HelvetiaTool *tool = g_ptr_array_index(all_tools, i);
        gboolean match = FALSE;
        
        if (tool->name) {
            char *lower_name = g_utf8_strdown(tool->name, -1);
            if (strstr(lower_name, lower_query)) match = TRUE;
            g_free(lower_name);
        }
        
        if (!match && tool->description) {
            char *lower_desc = g_utf8_strdown(tool->description, -1);
            if (strstr(lower_desc, lower_query)) match = TRUE;
            g_free(lower_desc);
        }
        
        if (!match && tool->keywords) {
            for (int k = 0; tool->keywords[k] != NULL; k++) {
                char *lower_kw = g_utf8_strdown(tool->keywords[k], -1);
                if (strstr(lower_kw, lower_query)) {
                    match = TRUE;
                    g_free(lower_kw);
                    break;
                }
                g_free(lower_kw);
            }
        }
        
        if (match) {
            g_ptr_array_add(results, (gpointer)tool);
            if (results->len >= max_results) break;
        }
    }
    
    g_free(lower_query);
    return results;
}

const HelvetiaTool *helvetia_tool_registry_find_by_cli(const char *cli_command) {
    if (!all_tools || !cli_command) return NULL;
    
    for (guint i = 0; i < all_tools->len; i++) {
        const HelvetiaTool *tool = g_ptr_array_index(all_tools, i);
        if (tool->cli_command && strcmp(tool->cli_command, cli_command) == 0) {
            return tool;
        }
    }
    return NULL;
}
