#include "plugin_loader.h"
#include "module_registry.h"
#include <gmodule.h>
#include <dirent.h>
#include <string.h>

static GPtrArray *g_handles    = NULL;   /* open GModule* handles */
static char      *g_plugin_dir = NULL;

static void close_module(gpointer p) { g_module_close((GModule *)p); }

void helvetia_plugin_loader_init(const char *plugin_dir) {
    g_plugin_dir = g_strdup(plugin_dir);
    if (!g_handles)
        g_handles = g_ptr_array_new_with_free_func(close_module);
}

static void load_one(const char *path) {
    GModule *mod = g_module_open(path, G_MODULE_BIND_LAZY);
    if (!mod) {
        g_warning("plugin: cannot open %s: %s", path, g_module_error());
        return;
    }

    helvetia_plugin_get_module_fn get_module = NULL;
    if (!g_module_symbol(mod, HELVETIA_PLUGIN_ENTRY_SYMBOL, (gpointer *)&get_module)) {
        g_warning("plugin: %s is missing the entry symbol '%s'",
                  path, HELVETIA_PLUGIN_ENTRY_SYMBOL);
        g_module_close(mod);
        return;
    }

    HelvetiaModule *m = get_module(HELVETIA_PLUGIN_API_VERSION);
    if (!m || !m->id || !m->subcategories) {
        g_warning("plugin: %s returned an invalid module descriptor", path);
        g_module_close(mod);
        return;
    }

    helvetia_module_registry_add(m);
    g_ptr_array_add(g_handles, mod);   /* keep handle alive */
    g_message("plugin loaded: '%s'  (%s)", m->id, path);
}

void helvetia_plugin_loader_load_all(void) {
    if (!g_plugin_dir) return;

    DIR *d = opendir(g_plugin_dir);
    if (!d) {
        g_debug("plugin dir not found: %s — skipping", g_plugin_dir);
        return;
    }

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        const char *name = ent->d_name;
        size_t len = strlen(name);
        /* Only load files that end with ".so" */
        if (len < 4 || strcmp(name + len - 3, ".so") != 0) continue;

        char *full = g_build_filename(g_plugin_dir, name, NULL);
        load_one(full);
        g_free(full);
    }
    closedir(d);
}

void helvetia_plugin_loader_shutdown(void) {
    if (g_handles) {
        g_ptr_array_free(g_handles, TRUE);
        g_handles = NULL;
    }
    g_clear_pointer(&g_plugin_dir, g_free);
}
