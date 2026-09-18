#include <gtk/gtk.h>

static void app_activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *win = gtk_application_window_new(app);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    
    GtkWidget *color = gtk_color_chooser_widget_new();
    gtk_box_append(GTK_BOX(box), color);
    
    gtk_window_set_child(GTK_WINDOW(win), box);
    gtk_window_present(GTK_WINDOW(win));
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("org.gtk.testcolor", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(app_activate), NULL);
    return g_application_run(G_APPLICATION(app), argc, argv);
}
