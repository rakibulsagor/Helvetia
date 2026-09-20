#pragma once
#include <gtk/gtk.h>

#define HELVETIA_PLUGIN_API_VERSION 1

/**
 * A single command a tool supports.
 *
 * Commands are invoked through "win.tool_action" with the tool's ID
 * and this command's ID. The handler receives the tool's root view
 * widget so it can reach into the tool's internal state.
 */
typedef struct {
    const char *id;             /* e.g. "export", "reset", "apply"     */
    const char *name;           /* display name: "Export…"             */
    const char *icon_name;      /* optional symbolic icon              */
    const char *accel;          /* optional accelerator, e.g. "<Control>e" */
    const char *tooltip;        /* optional tooltip                    */
    void (*activate)(GtkWidget *tool_view);   /* handler */
} HelvetiaToolCommand;

/**
 * HelvetiaTool — defines a single tool (e.g. "Base64 Encode").
 */
typedef struct {
    const char *id;           /* e.g. "base64_encode" */
    const char *name;         /* e.g. "Base64 Encode" */
    const char *description;  /* short summary */
    const char *icon_name;    /* GNOME symbolic icon, e.g. "text-x-generic-symbolic" */
    const char **keywords;    /* NULL-terminated array of search keywords */
    const char *cli_command;  /* e.g. "b64enc" */
    
    /**
     * Called to create the main UI widget for this tool.
     * If NULL, the tool is considered "Coming Soon".
     */
    GtkWidget *(*create_view)(void);
    
    void (*on_open)(GtkWidget *view);
    void (*on_close)(GtkWidget *view);

    /* NULL-terminated list of commands this tool supports */
    const HelvetiaToolCommand *commands;

    const char *(*file_filter_name)(void);
    const char *(*file_filter_pattern)(void);
} HelvetiaTool;

/**
 * HelvetiaSubcategory — groups tools within a module.
 */
typedef struct {
    const char *name;         /* e.g. "Generators" */
    const HelvetiaTool *tools; /* NULL-terminated array of tools */
} HelvetiaSubcategory;

/**
 * HelvetiaModule — the single interface every built-in and external module
 * must implement.  A "module" is one tab / workspace in the sidebar.
 */
typedef struct {
    const char *id;           /* unique, e.g. "utility"         */
    const char *name;         /* shown in the sidebar           */
    const char *icon_name;    /* GTK icon name; may be NULL     */
    const char *description;  /* one-line tooltip               */

    /* NULL-terminated array of subcategories in this module */
    const HelvetiaSubcategory *subcategories;

    /**
     * Optional: Called to create a custom main view for this module.
     * If provided, the UI will use this widget instead of auto-generating
     * a grid from `subcategories`.
     */
    GtkWidget *(*create_view)(void);

    /* Optional lifecycle callbacks */
    void (*on_activate)(void);    /* tab becomes visible           */
    void (*on_deactivate)(void);  /* tab is hidden                 */
    void (*on_shutdown)(void);    /* app is closing                */
} HelvetiaModule;

/**
 * Every external plugin .so must export exactly this symbol.
 * api_version will be HELVETIA_PLUGIN_API_VERSION at call-time.
 * Return NULL to refuse loading (e.g. version mismatch).
 */
typedef HelvetiaModule *(*helvetia_plugin_get_module_fn)(int api_version);

#define HELVETIA_PLUGIN_ENTRY_SYMBOL "helvetia_plugin_get_module"
