#include "qpdf_wrapper.h"

extern "C" {

gboolean qpdf_merge_files(const char *const *inputs, guint n_inputs,
                          const char *output, GError **error) {
    (void)inputs;
    (void)n_inputs;
    (void)output;
    (void)error;
    /* Simulate work */
    g_usleep(1000000);
    return TRUE;
}

gboolean qpdf_split_file(const char *input, const char *output_dir,
                         const char *ranges, GError **error) {
    (void)input;
    (void)output_dir;
    (void)ranges;
    (void)error;
    /* Simulate work */
    g_usleep(1000000);
    return TRUE;
}

gboolean qpdf_rotate_pages(const char *input, const char *output,
                           int angle, const char *pages, GError **error) {
    (void)input;
    (void)output;
    (void)angle;
    (void)pages;
    (void)error;
    /* Simulate work */
    g_usleep(1000000);
    return TRUE;
}

gboolean qpdf_compress_file(const char *input, const char *output,
                            const char *quality, gboolean preserve_metadata, GError **error) {
    (void)input;
    (void)output;
    (void)quality;
    (void)preserve_metadata;
    (void)error;
    /* Simulate work */
    g_usleep(1000000);
    return TRUE;
}

} /* extern "C" */
