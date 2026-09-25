#include "gegl_wrapper.h"
#include <gegl.h>
#include <babl/babl.h>

void helvetia_gegl_init(void) { gegl_init(NULL, NULL); }

GeglBuffer *gegl_load_image(const char *path, GError **error) {
    GeglNode *g = gegl_node_new();
    GeglNode *load = gegl_node_new_child(g,
        "operation", "gegl:load", "path", path, NULL);
    gegl_node_process(load);
    GeglBuffer *buf = gegl_node_get_output_buffer(load, "output");
    if (buf) g_object_ref(buf);
    else g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                     "Load failed: %s", path);
    g_object_unref(g);
    return buf;
}

GeglBuffer *gegl_apply(GeglBuffer *in, const char *operation,
                       GHashTable *props, GError **error) {
    (void)error;
    GeglNode *g = gegl_node_new();
    GeglNode *op = gegl_node_new_child(g, "operation", operation, NULL);

    if (props) {
        GHashTableIter it; gpointer k, v;
        g_hash_table_iter_init(&it, props);
        while (g_hash_table_iter_next(&it, &k, &v))
            gegl_node_set(op, k, v, NULL);
    }
    gegl_node_set(op, "buffer", in, NULL);
    gegl_node_process(op);
    GeglBuffer *out = gegl_node_get_output_buffer(op, "output");
    if (out) g_object_ref(out);
    g_object_unref(g);
    return out;
}

GdkPixbuf *gegl_to_pixbuf(GeglBuffer *buf, GError **error) {
    (void)error;
    GeglRectangle r = gegl_buffer_get_extent(buf);
    guchar *px = g_malloc(r.width * r.height * 4);
    gegl_buffer_get(buf, &r, 1.0, babl_format("R'G'B'A u8"),
                    px, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
    return gdk_pixbuf_new_from_data(px, GDK_COLORSPACE_RGB, TRUE, 8,
                                    r.width, r.height, r.width * 4,
                                    (GdkPixbufDestroyNotify)g_free, NULL);
}

gboolean gegl_save_image(GeglBuffer *buf, const char *path, GError **error) {
    (void)error;
    GeglNode *g = gegl_node_new();
    GeglNode *save = gegl_node_new_child(g,
        "operation", "gegl:save", "path", path, NULL);
    gegl_node_set(save, "buffer", buf, NULL);
    gegl_node_process(save);
    g_object_unref(g);
    return TRUE;
}
