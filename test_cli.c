#include <stdio.h>
#include "src/core/tool_registry.h"
#include "src/core/module_registry.h"

void helvetia_register_builtin_modules(void);

int main() {
    helvetia_tool_registry_init();
    helvetia_module_registry_init();
    helvetia_register_builtin_modules();
    
    const HelvetiaTool *tool = helvetia_tool_registry_find_by_cli("uuid-generator");
    if (tool) {
        printf("Found: %s\n", tool->name);
    } else {
        printf("Not found\n");
    }
    return 0;
}
