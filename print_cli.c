#include <stdio.h>
#include "src/core/tool_registry.h"
#include "src/core/module_registry.h"
#include <glib.h>

void helvetia_register_builtin_modules(void);

int main() {
    helvetia_tool_registry_init();
    helvetia_module_registry_init();
    helvetia_register_builtin_modules();
    
    GPtrArray *tools = helvetia_tool_registry_get_all();
    for (guint i = 0; i < tools->len; i++) {
        const HelvetiaTool *t = g_ptr_array_index(tools, i);
        if (t->cli_command) {
            printf("%s\n", t->cli_command);
        }
    }
    return 0;
}
