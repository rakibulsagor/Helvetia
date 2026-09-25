#include "image_editor_shell.h"

GtkWidget *image_editor_shell_new(ImageEditorSlots *out) {
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    /* ---- Top: three-column body ---- */
    GtkWidget *body = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_vexpand(body, TRUE);

    /* Left palette — 56px wide */
    GtkWidget *left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_size_request(left, 56, -1);
    gtk_widget_set_margin_start(left, 8);
    gtk_widget_set_margin_end(left, 8);
    gtk_widget_set_margin_top(left, 8);
    gtk_widget_set_margin_bottom(left, 8);
    gtk_widget_add_css_class(left, "editor-palette");

    /* Center canvas */
    GtkWidget *center = gtk_overlay_new();
    gtk_widget_set_hexpand(center, TRUE);
    gtk_widget_set_vexpand(center, TRUE);
    gtk_widget_add_css_class(center, "editor-canvas");

    /* Right panels — 280px wide */
    GtkWidget *right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_size_request(right, 280, -1);
    gtk_widget_set_margin_start(right, 8);
    gtk_widget_set_margin_end(right, 8);
    gtk_widget_set_margin_top(right, 8);
    gtk_widget_set_margin_bottom(right, 8);
    gtk_widget_add_css_class(right, "editor-panels");

    gtk_box_append(GTK_BOX(body), left);
    gtk_box_append(GTK_BOX(body), center);
    gtk_box_append(GTK_BOX(body), right);

    /* ---- Bottom bar ---- */
    GtkWidget *bottom = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_set_size_request(bottom, -1, 28);
    gtk_widget_set_margin_start(bottom, 12);
    gtk_widget_set_margin_end(bottom, 12);
    gtk_widget_add_css_class(bottom, "editor-bottom");

    gtk_box_append(GTK_BOX(root), body);
    gtk_box_append(GTK_BOX(root), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(root), bottom);

    if (out) {
        out->left_palette  = left;
        out->center_canvas = center;
        out->right_panels  = right;
        out->bottom_bar    = bottom;
    }
    return root;
}
