#pragma once
#include "../../core/plugin.h"

G_BEGIN_DECLS

/* Tool view creators */
GtkWidget *pdf_merge_create_view(void);
GtkWidget *pdf_split_create_view(void);
GtkWidget *pdf_compress_create_view(void);

/* Command arrays */
extern const HelvetiaToolCommand pdf_merge_cmds[];
extern const HelvetiaToolCommand pdf_split_cmds[];
extern const HelvetiaToolCommand pdf_compress_cmds[];
const HelvetiaTool *pdf_tool_extract_images(void);
const HelvetiaTool *pdf_tool_watermark(void);
const HelvetiaTool *pdf_tool_encrypt(void);

G_END_DECLS
