#pragma once
#include <glib.h>
#include <gtk/gtk.h>
#include <gegl.h>

G_BEGIN_DECLS

void        helvetia_gegl_init(void);
GeglBuffer *gegl_load_image(const char *path, GError **error);
gboolean    gegl_save_image(GeglBuffer *buf, const char *path, GError **error);
GeglBuffer *gegl_apply(GeglBuffer *in, const char *operation,
                       GHashTable *props, GError **error);
GdkPixbuf  *gegl_to_pixbuf(GeglBuffer *buf, GError **error);

G_END_DECLS
