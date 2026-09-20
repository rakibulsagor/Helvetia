#include "image_processing.h"
#include <libraw/libraw.h>
#include <string.h>

unsigned char* decode_raw_file(const char *filename, int *out_width, int *out_height) {
    libraw_data_t *raw_data = libraw_init(0);
    if (!raw_data) {
        g_warning("Failed to initialize LibRaw");
        return NULL;
    }

    // 1. Open the file
    int ret = libraw_open_file(raw_data, filename);
    if (ret != LIBRAW_SUCCESS) {
        g_warning("LibRaw: Cannot open %s: %s", filename, libraw_strerror(ret));
        libraw_close(raw_data);
        return NULL;
    }

    // 2. Unpack the raw data
    ret = libraw_unpack(raw_data);
    if (ret != LIBRAW_SUCCESS) {
        g_warning("LibRaw: Cannot unpack %s: %s", filename, libraw_strerror(ret));
        libraw_close(raw_data);
        return NULL;
    }

    // 3. Process the image (demosaicing, white balance, etc.)
    ret = libraw_dcraw_process(raw_data);
    if (ret != LIBRAW_SUCCESS) {
        g_warning("LibRaw: Cannot process %s: %s", filename, libraw_strerror(ret));
        libraw_close(raw_data);
        return NULL;
    }

    // 4. Get the processed image in memory as a simple 8-bit bitmap (BMP-like)
    int err;
    libraw_processed_image_t *image = libraw_dcraw_make_mem_image(raw_data, &err);
    if (!image) {
        g_warning("LibRaw: Cannot create in-memory image: %s", libraw_strerror(err));
        libraw_close(raw_data);
        return NULL;
    }

    // 5. Copy the pixel data into a new buffer for GEGL
    // Note: LibRaw returns packed RGB data (R,G,B,R,G,B,...)
    int width = image->width;
    int height = image->height;
    size_t buffer_size = (size_t)width * (size_t)height * 3; // 3 bytes per pixel (RGB)
    unsigned char *pixel_buffer = g_malloc(buffer_size);
    memcpy(pixel_buffer, image->data, buffer_size);

    // 6. Clean up
    if (out_width) *out_width = width;
    if (out_height) *out_height = height;
    libraw_dcraw_clear_mem(image);
    libraw_close(raw_data);

    return pixel_buffer;
}

GeglBuffer* process_with_gegl(unsigned char *pixels, int width, int height) {
    // 1. Create a GEGL buffer from the LibRaw pixel data
    // The format "R'G'B' u8" is standard 8-bit sRGB.
    GeglBuffer *input_buffer = gegl_buffer_new(GEGL_RECTANGLE(0, 0, width, height), babl_format("R'G'B' u8"));
    gegl_buffer_set(input_buffer, GEGL_RECTANGLE(0, 0, width, height), 0, babl_format("R'G'B' u8"), pixels, GEGL_AUTO_ROWSTRIDE);

    // 2. Build the GEGL graph
    // This graph takes an input, resizes it to half, and then sharpens it.
    GeglNode *graph = gegl_node_new();
    GeglNode *input = gegl_node_new_child(graph, "operation", "gegl:buffer-source", "buffer", input_buffer, NULL);
    GeglNode *resize = gegl_node_new_child(graph, "operation", "gegl:scale-size", "x", 0.5, "y", 0.5, NULL);
    GeglNode *sharpen = gegl_node_new_child(graph, "operation", "gegl:unsharp-mask", "std-dev", 3.0, NULL);
    GeglNode *output = gegl_node_new_child(graph, "operation", "gegl:buffer-sink", "buffer", NULL, NULL);

    // Link the nodes
    gegl_node_link_many(input, resize, sharpen, output, NULL);

    // 3. Process the graph
    // This computes the final image into a new buffer.
    gegl_node_process(output);

    // 4. Retrieve the result
    GeglBuffer *output_buffer = NULL;
    gegl_node_get(output, "buffer", &output_buffer, NULL);

    // 5. Clean up
    g_object_unref(graph); // This also unrefs all child nodes, including the input_buffer.
    // NOTE: Do NOT unref output_buffer; the caller is responsible for it.

    return output_buffer;
}
