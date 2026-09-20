#pragma once

#include <glib.h>
#include <gegl.h>

G_BEGIN_DECLS

/**
 * decode_raw_file:
 * @filename: path to the RAW file
 * @out_width: (out): return location for the image width
 * @out_height: (out): return location for the image height
 *
 * Decodes a RAW image file into a newly allocated 8-bit RGB buffer.
 *
 * Returns: (transfer full): the RGB pixel buffer, or NULL on error.
 */
unsigned char* decode_raw_file(const char *filename, int *out_width, int *out_height);

/**
 * process_with_gegl:
 * @pixels: input 8-bit RGB pixel buffer
 * @width: width of the image
 * @height: height of the image
 *
 * Processes a pixel buffer using a GEGL graph.
 *
 * Returns: (transfer full): the output GEGL buffer, or NULL on error.
 */
GeglBuffer* process_with_gegl(unsigned char *pixels, int width, int height);

G_END_DECLS
