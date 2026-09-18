#include <gtk/gtk.h>
int main() {
    GtkStringList *sl = gtk_string_list_new(NULL);
    const char *additions[] = {"Hello", "World", NULL};
    gtk_string_list_splice(sl, 0, 0, additions);
    return 0;
}
