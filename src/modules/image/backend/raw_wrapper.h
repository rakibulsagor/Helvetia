#pragma once
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib.h>

G_BEGIN_DECLS

GdkPixbuf *raw_to_pixbuf(const char *path, GError **error);

G_END_DECLS
