/* ================================================================
 * Helvetia — Shared Widget Helpers
 * ================================================================ */
#pragma once
#include <gtk/gtk.h>

GtkWidget *hv_make_result_label   (void);
GtkWidget *hv_make_entry_row      (const char *label, GtkWidget **entry_out);
GtkWidget *hv_make_copy_btn       (GtkWidget *source_label);
GtkWidget *hv_make_action_btn     (const char *label);
GtkWidget *hv_make_text_view      (GtkWidget **textview_out, gboolean editable);
void        hv_textview_set_text  (GtkWidget *tv, const char *text);
char       *hv_textview_get_text  (GtkWidget *tv);
GtkWidget  *hv_make_separator     (void);
char       *hv_run_cmd            (const char *cmd);
GtkWidget  *hv_make_file_picker_row(const char *label, GtkWidget **entry_out, gboolean is_save);
