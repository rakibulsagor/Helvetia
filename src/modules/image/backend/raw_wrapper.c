#include "raw_wrapper.h"
#include <libraw/libraw.h>

GdkPixbuf *raw_to_pixbuf(const char *path, GError **error) {
    libraw_data_t *raw = libraw_init(0);
    if (!raw) { g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                            "LibRaw init failed"); return NULL; }

    if (libraw_open_file(raw, path) != LIBRAW_SUCCESS ||
        libraw_unpack(raw) != LIBRAW_SUCCESS ||
        libraw_dcraw_process(raw) != LIBRAW_SUCCESS) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                    "RAW decode failed: %s", path);
        libraw_close(raw);
        return NULL;
    }

    int err;
    libraw_processed_image_t *img = libraw_dcraw_make_mem_image(raw, &err);
    GdkPixbuf *pb = NULL;
    if (img && img->colors == 3) {
        pb = gdk_pixbuf_new_from_data(
            g_memdup2(img->data, img->data_size),
            GDK_COLORSPACE_RGB, FALSE, 8,
            img->width, img->height, img->width * 3,
            (GdkPixbufDestroyNotify)g_free, NULL);
    } else {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                    "RAW → pixbuf failed");
    }

    if (img) libraw_dcraw_clear_mem(img);
    libraw_close(raw);
    return pb;
}
