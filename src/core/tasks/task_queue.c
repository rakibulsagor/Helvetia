#include "task_queue.h"

struct _HelvetiaTaskQueue {
    GObject    parent_instance;
    GListStore *model;
    guint      active_count;
};

G_DEFINE_FINAL_TYPE(HelvetiaTaskQueue, helvetia_task_queue, G_TYPE_OBJECT)

enum {
    SIGNAL_TASK_ADDED,
    SIGNAL_TASK_COMPLETED,
    SIGNAL_ALL_DONE,
    N_SIGNALS,
};
static guint signals[N_SIGNALS];

static HelvetiaTaskQueue *g_default_queue = NULL;

/* ------------------------------------------------------------------ */
/* Task completion                                                    */
/* ------------------------------------------------------------------ */

static void on_task_completed(HelvetiaTask *task, const char *error,
                              gpointer user_data) {
    (void)error;
    HelvetiaTaskQueue *self = user_data;

    if (self->active_count > 0)
        self->active_count--;

    g_signal_emit(self, signals[SIGNAL_TASK_COMPLETED], 0, task);

    if (self->active_count == 0)
        g_signal_emit(self, signals[SIGNAL_ALL_DONE], 0);
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

HelvetiaTaskQueue *helvetia_task_queue_new(void) {
    return g_object_new(HELVETIA_TYPE_TASK_QUEUE, NULL);
}

HelvetiaTaskQueue *helvetia_task_queue_get_default(void) {
    if (!g_default_queue)
        g_default_queue = helvetia_task_queue_new();
    return g_default_queue;
}

void helvetia_task_queue_submit(HelvetiaTaskQueue *self, HelvetiaTask *task) {
    g_return_if_fail(HELVETIA_IS_TASK_QUEUE(self));
    g_return_if_fail(HELVETIA_IS_TASK(task));

    /* The model takes a reference; we keep the same ref for our own use. */
    g_list_store_append(self->model, task);
    g_signal_emit(self, signals[SIGNAL_TASK_ADDED], 0, task);

    self->active_count++;

    g_signal_connect(task, "completed", G_CALLBACK(on_task_completed), self);
    helvetia_task_start(task);

    g_object_unref(task);   /* ownership transferred to the model */
}

void helvetia_task_queue_cancel_all(HelvetiaTaskQueue *self) {
    guint n = g_list_model_get_n_items(G_LIST_MODEL(self->model));
    for (guint i = 0; i < n; i++) {
        HelvetiaTask *t = g_list_model_get_item(G_LIST_MODEL(self->model), i);
        if (helvetia_task_get_status(t) == HELVETIA_TASK_RUNNING ||
            helvetia_task_get_status(t) == HELVETIA_TASK_PENDING) {
            helvetia_task_cancel(t);
        }
        g_object_unref(t);
    }
}

void helvetia_task_queue_clear_completed(HelvetiaTaskQueue *self) {
    for (gint i = (gint)g_list_model_get_n_items(G_LIST_MODEL(self->model)) - 1;
         i >= 0; i--) {
        HelvetiaTask *t = g_list_model_get_item(G_LIST_MODEL(self->model), (guint)i);
        HelvetiaTaskStatus s = helvetia_task_get_status(t);
        g_object_unref(t);
        if (s == HELVETIA_TASK_DONE ||
            s == HELVETIA_TASK_FAILED ||
            s == HELVETIA_TASK_CANCELLED) {
            g_list_store_remove(self->model, (guint)i);
        }
    }
}

guint helvetia_task_queue_active_count(HelvetiaTaskQueue *self) {
    return self->active_count;
}

GListModel *helvetia_task_queue_get_model(HelvetiaTaskQueue *self) {
    return G_LIST_MODEL(self->model);
}

/* ------------------------------------------------------------------ */
/* GObject boilerplate                                                */
/* ------------------------------------------------------------------ */

static void helvetia_task_queue_dispose(GObject *obj) {
    HelvetiaTaskQueue *self = HELVETIA_TASK_QUEUE(obj);
    g_clear_object(&self->model);
    G_OBJECT_CLASS(helvetia_task_queue_parent_class)->dispose(obj);
}

static void helvetia_task_queue_class_init(HelvetiaTaskQueueClass *klass) {
    GObjectClass *obj_class = G_OBJECT_CLASS(klass);
    obj_class->dispose = helvetia_task_queue_dispose;

    signals[SIGNAL_TASK_ADDED] =
        g_signal_new("task-added", HELVETIA_TYPE_TASK_QUEUE,
                     G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                     G_TYPE_NONE, 1, HELVETIA_TYPE_TASK);

    signals[SIGNAL_TASK_COMPLETED] =
        g_signal_new("task-completed", HELVETIA_TYPE_TASK_QUEUE,
                     G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                     G_TYPE_NONE, 1, HELVETIA_TYPE_TASK);

    signals[SIGNAL_ALL_DONE] =
        g_signal_new("all-done", HELVETIA_TYPE_TASK_QUEUE,
                     G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                     G_TYPE_NONE, 0);
}

static void helvetia_task_queue_init(HelvetiaTaskQueue *self) {
    self->model = g_list_store_new(HELVETIA_TYPE_TASK);
    self->active_count = 0;
}