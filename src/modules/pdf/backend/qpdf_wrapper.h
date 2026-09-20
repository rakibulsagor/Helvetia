#pragma once
#include <glib.h>

G_BEGIN_DECLS

gboolean qpdf_merge_files  (const char *const *inputs, guint n_inputs,
                            const char *output, GError **error);
gboolean qpdf_split_file   (const char *input, const char *output_dir,
                            const char *ranges, GError **error);
gboolean qpdf_rotate_pages (const char *input, const char *output,
                            int angle, const char *pages, GError **error);
gboolean qpdf_compress_file(const char *input, const char *output,
                            const char *quality, gboolean preserve_metadata, GError **error);

G_END_DECLS
