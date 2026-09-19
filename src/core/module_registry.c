#include "module_registry.h"
#include "tool_registry.h"
#include <glib.h>

static GPtrArray *g_modules = NULL;

void helvetia_module_registry_init(void) {
    if (!g_modules)
        g_modules = g_ptr_array_new_with_free_func(NULL);
}

void helvetia_module_registry_shutdown(void) {
    if (g_modules) {
        g_ptr_array_free(g_modules, TRUE);
        g_modules = NULL;
    }
}

void helvetia_module_registry_add(const HelvetiaModule *module) {
    g_return_if_fail(module && module->id);
    if (!g_modules) helvetia_module_registry_init();

    /* Prevent duplicate ids */
    if (helvetia_module_registry_find(module->id)) return;
    g_ptr_array_add(g_modules, (gpointer)module);
    /*
     * Keep the tool index in lockstep with the module registry.  Previously
     * this happened while constructing the first window, which meant CLI
     * dispatch ran against an empty index and creating another window added
     * every tool a second time.
     */
    helvetia_tool_registry_index_module(module);
}

guint helvetia_module_registry_count(void) {
    return g_modules ? g_modules->len : 0;
}

const HelvetiaModule *helvetia_module_registry_get(guint index) {
    if (!g_modules || index >= g_modules->len) return NULL;
    return g_ptr_array_index(g_modules, index);
}

const HelvetiaModule *helvetia_module_registry_find(const char *id) {
    if (!g_modules || !id) return NULL;
    for (guint i = 0; i < g_modules->len; i++) {
        const HelvetiaModule *m = g_ptr_array_index(g_modules, i);
        if (g_strcmp0(m->id, id) == 0) return m;
    }
    return NULL;
}
