#pragma once
#include "plugin.h"

/**
 * Module registry — a central list of every module (built-in or plugin)
 * that Helvetia knows about at runtime.
 */
void              helvetia_module_registry_init(void);
void              helvetia_module_registry_shutdown(void);

/** Register a module. Duplicate ids are silently ignored. */
void              helvetia_module_registry_add(const HelvetiaModule *module);

/** Query the registry. */
guint             helvetia_module_registry_count(void);
const HelvetiaModule *helvetia_module_registry_get(guint index);
const HelvetiaModule *helvetia_module_registry_find(const char *id);

/** Register all built-in (statically-linked) modules. */
void              helvetia_register_builtin_modules(void);
