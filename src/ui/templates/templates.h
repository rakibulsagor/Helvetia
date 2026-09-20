#pragma once
#include <gtk/gtk.h>

G_BEGIN_DECLS

/* =========================================================================
 * 1. File Processor Template
 * Flow: Upload -> Options -> Action -> Preview -> Download
 * ========================================================================= */
typedef struct {
    const char *tool_id;             /* The ID of the tool (e.g. "pdf.merge") */
    const char *file_type_hint;      /* e.g., "PDF file" */
    
    /* Command ID for the primary button, handled via win.tool_action */
    const char *primary_action_id;   
    const char *primary_action_label; 
    
    /* Optional: creates the widget holding sliders, dropdowns, etc. */
    GtkWidget *(*create_options_widget)(void);
    
    /* Optional: Called when the user drops/selects a file. The tool can read this path. */
    void (*on_file_loaded)(const char *path, gpointer state);
    
    /* The template allocates state for the tool if size > 0.
       Accessible via g_object_get_data(view, "tool-state") */
    gsize state_size;
} HelvetiaFileProcessorConfig;

GtkWidget *helvetia_template_file_processor_new(const HelvetiaFileProcessorConfig *config);

/* =========================================================================
 * 2. Generator Template
 * Flow: Inputs -> Output -> Copy
 * ========================================================================= */
typedef struct {
    const char *tool_id;
    GtkWidget *(*create_inputs_widget)(void);
    const char *primary_action_id;
    const char *primary_action_label;
    gsize state_size;
} HelvetiaGeneratorConfig;

GtkWidget *helvetia_template_generator_new(const HelvetiaGeneratorConfig *config);

/* =========================================================================
 * 3. Converter Template
 * Flow: Input Panel <-> Output Panel
 * ========================================================================= */
typedef struct {
    const char *tool_id;
    gboolean is_live; /* TRUE for text (live updates), FALSE for files (button) */
    GtkWidget *(*create_options_widget)(void);
    const char *primary_action_id; 
    const char *primary_action_label;
    gsize state_size;
} HelvetiaConverterConfig;

GtkWidget *helvetia_template_converter_new(const HelvetiaConverterConfig *config);

/* =========================================================================
 * 4. Lookup Template
 * Flow: Query -> Result Table
 * ========================================================================= */
typedef struct {
    const char *tool_id;
    const char *query_placeholder;
    const char *primary_action_id; 
    gsize state_size;
} HelvetiaLookupConfig;

GtkWidget *helvetia_template_lookup_new(const HelvetiaLookupConfig *config);

/* =========================================================================
 * 5. Viewer/Editor Template
 * Flow: Load -> Full Panel
 * ========================================================================= */
typedef struct {
    const char *tool_id;
    const char *file_type_hint;
    gboolean is_readonly;
    GtkWidget *(*create_editor_widget)(void);
    void (*on_file_loaded)(const char *path, gpointer state);
    gsize state_size;
} HelvetiaViewerConfig;

GtkWidget *helvetia_template_viewer_new(const HelvetiaViewerConfig *config);

/* =========================================================================
 * 6. Calculator Template
 * Flow: Fields -> Live Result
 * ========================================================================= */
typedef struct {
    const char *tool_id;
    GtkWidget *(*create_fields_widget)(void);
    gsize state_size;
} HelvetiaCalculatorConfig;

GtkWidget *helvetia_template_calculator_new(const HelvetiaCalculatorConfig *config);

G_END_DECLS
