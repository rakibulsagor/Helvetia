#pragma once
#include "plugin.h"

/**
 * Plugin loader — scans a directory for .so files and tries to load each
 * one as a Helvetia plugin via the HELVETIA_PLUGIN_ENTRY_SYMBOL entry point.
 */
void helvetia_plugin_loader_init(const char *plugin_dir);
void helvetia_plugin_loader_load_all(void);
void helvetia_plugin_loader_shutdown(void);
