#include "task_strip.h"

struct _HelvetiaTaskStrip {
    GtkBox parent_instance;
    HelvetiaTaskQueue *queue;
    GtkListBox *list;
};

G_DEFINE_FINAL_TYPE(HelvetiaTaskStrip, helvetia_task_strip, GTK_TYPE_BOX)

/* ------------------------------------------------------------------ */
/* Row builder                                                        */
/* ------------------------------------------------------------------ */

static void on_cancel_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    HelvetiaTask *task = user_data;
    helvetia_task_cancel(task);
}

static void on_progress_changed(HelvetiaTask *task, double fraction,
                                gpointer user_data) {
    (void)task;
    GtkProgressBar *pb = user_data;
    gtk_progress_bar_set_fraction(pb, fraction);
}

static GtkWidget *build_task_row(HelvetiaTask *task) {
    GtkWidget *row_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_start(row_box, 12);
    gtk_widget_set_margin_end(row_box, 12);
    gtk_widget_set_margin_top(row_box, 6);
    gtk_widget_set_margin_bottom(row_box, 6);

    GtkWidget *icon = gtk_image_new_from_icon_name("emblem-synchronizing-symbolic");

    GtkWidget *text_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_hexpand(text_box, TRUE);
    gtk_widget_set_valign(text_box, GTK_ALIGN_CENTER);

    GtkWidget *name = gtk_label_new(helvetia_task_get_name(task));
    gtk_label_set_xalign(GTK_LABEL(name), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(name), PANGO_ELLIPSIZE_END);

    GtkWidget *progress = gtk_progress_bar_new();
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress),
                                   helvetia_task_get_progress(task));
    gtk_widget_set_hexpand(progress, TRUE);

    gtk_box_append(GTK_BOX(text_box), name);
    gtk_box_append(GTK_BOX(text_box), progress);

    GtkWidget *cancel = gtk_button_new_from_icon_name("window-close-symbolic");
    gtk_widget_add_css_class(cancel, "flat");
    gtk_widget_set_tooltip_text(cancel, "Cancel");
    gtk_widget_set_valign(cancel, GTK_ALIGN_CENTER);
    g_signal_connect(cancel, "clicked", G_CALLBACK(on_cancel_clicked), task);

    gtk_box_append(GTK_BOX(row_box), icon);
    gtk_box_append(GTK_BOX(row_box), text_box);
    gtk_box_append(GTK_BOX(row_box), cancel);

    /* Bind progress updates */
    g_object_set_data(G_OBJECT(row_box), "task", task);
    g_object_set_data(G_OBJECT(row_box), "progress", progress);

    g_signal_connect_object(task, "progress-changed",
        G_CALLBACK(on_progress_changed), progress, 0);

    return row_box;
}

/* ------------------------------------------------------------------ */
/* Model binding                                                      */
/* ------------------------------------------------------------------ */

static void on_items_changed(GListModel *model, guint position,
                             guint removed, guint added, gpointer user_data) {
    (void)removed;
    (void)added;
    HelvetiaTaskStrip *self = user_data;

    /* Rebuild the list box — small N, so it's cheap */
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(GTK_WIDGET(self->list))))
        gtk_list_box_remove(self->list, child);

    guint n = g_list_model_get_n_items(model);
    for (guint i = 0; i < n; i++) {
        HelvetiaTask *t = g_list_model_get_item(model, i);
        GtkWidget *row = build_task_row(t);
        GtkWidget *lb_row = gtk_list_box_row_new();
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(lb_row), row);
        gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(lb_row), FALSE);
        gtk_list_box_append(self->list, lb_row);
        g_object_unref(t);
    }

    gtk_widget_set_visible(GTK_WIDGET(self), n > 0);
    (void)position;
}

/* ------------------------------------------------------------------ */
/* GObject boilerplate                                                */
/* ------------------------------------------------------------------ */

static void helvetia_task_strip_dispose(GObject *obj) {
    HelvetiaTaskStrip *self = HELVETIA_TASK_STRIP(obj);
    g_clear_object(&self->queue);
    G_OBJECT_CLASS(helvetia_task_strip_parent_class)->dispose(obj);
}

static void helvetia_task_strip_class_init(HelvetiaTaskStripClass *klass) {
    G_OBJECT_CLASS(klass)->dispose = helvetia_task_strip_dispose;
}

static void helvetia_task_strip_init(HelvetiaTaskStrip *self) {
    gtk_orientable_set_orientation(GTK_ORIENTABLE(self), GTK_ORIENTATION_VERTICAL);

    self->list = GTK_LIST_BOX(gtk_list_box_new());
    gtk_list_box_set_selection_mode(self->list, GTK_SELECTION_NONE);
    gtk_widget_add_css_class(GTK_WIDGET(self->list), "task-strip");

    gtk_box_append(GTK_BOX(self), GTK_WIDGET(self->list));

    gtk_widget_set_visible(GTK_WIDGET(self), FALSE);
}

HelvetiaTaskStrip *helvetia_task_strip_new(HelvetiaTaskQueue *queue) {
    HelvetiaTaskStrip *self = g_object_new(HELVETIA_TYPE_TASK_STRIP, NULL);
    self->queue = g_object_ref(queue);

    GListModel *model = helvetia_task_queue_get_model(queue);
    g_signal_connect_object(model, "items-changed", G_CALLBACK(on_items_changed),
                            self, 0);
    return self;
}