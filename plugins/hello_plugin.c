/* ================================================================
 * Helvetia — Example external plugin
 * Build: gcc -shared -fPIC -o hello.so hello_plugin.c \
 *             $(pkg-config --cflags --libs gtk4 glib-2.0)
 * Drop the resulting hello.so into your plugin directory.
 * ================================================================ */
#include "../src/core/plugin.h"
#include <gtk/gtk.h>

static GtkWidget *hello_view(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start(box, 32);
    gtk_widget_set_margin_end(box, 32);
    gtk_widget_set_margin_top(box, 32);
    gtk_widget_set_margin_bottom(box, 32);

    GtkWidget *icon = gtk_image_new_from_icon_name("face-smile");
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 64);

    GtkWidget *lbl = gtk_label_new("Hello from an external plugin!");
    gtk_widget_add_css_class(lbl, "helvetia-section-title");

    GtkWidget *sub = gtk_label_new(
        "Drop any .so into the plugin directory and it will appear here.");
    gtk_widget_add_css_class(sub, "helvetia-section-desc");
    gtk_label_set_wrap(GTK_LABEL(sub), TRUE);

    gtk_box_append(GTK_BOX(box), icon);
    gtk_box_append(GTK_BOX(box), lbl);
    gtk_box_append(GTK_BOX(box), sub);
    return box;
}

static HelvetiaModule mod = {
    .id          = "hello",
    .name        = "Hello Plugin",
    .icon_name   = "face-smile",
    .description = "A minimal example external plugin.",
    .create_view = hello_view,
};

HelvetiaModule *helvetia_plugin_get_module(int api_version) {
    if (api_version != HELVETIA_PLUGIN_API_VERSION) return NULL;
    return &mod;
}
