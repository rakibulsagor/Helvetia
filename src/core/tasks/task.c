#include "task.h"
#include <glib.h>

struct _HelvetiaTask {
    GObject            parent_instance;

    char              *name;
    char              *tool_id;
    char              *status_msg;
    char              *error;

    HelvetiaTaskStatus status;
    double             progress;

    GTask             *gtask;
    GCancellable      *cancellable;
    GTaskThreadFunc    worker;
    gpointer           worker_data;
    GDestroyNotify     worker_data_free;

    gint64             started_at;
    gint64             finished_at;
};

G_DEFINE_FINAL_TYPE(HelvetiaTask, helvetia_task, G_TYPE_OBJECT)

enum {
    SIGNAL_PROGRESS_CHANGED,
    SIGNAL_STATUS_CHANGED,
    SIGNAL_COMPLETED,
    N_SIGNALS,
};
static guint signals[N_SIGNALS];

/* ------------------------------------------------------------------ */
/* Thread-safe marshalling helpers                                    */
/* ------------------------------------------------------------------ */

typedef struct {
    HelvetiaTask *task;
    double        fraction;
} ProgressPayload;

static gboolean emit_progress_idle(gpointer user_data) {
    ProgressPayload *p = user_data;
    p->task->progress = p->fraction;
    g_signal_emit(p->task, signals[SIGNAL_PROGRESS_CHANGED], 0, p->fraction);
    g_object_unref(p->task);
    g_free(p);
    return G_SOURCE_REMOVE;
}

typedef struct {
    HelvetiaTask *task;
    char         *message;
} StatusPayload;

static gboolean emit_status_idle(gpointer user_data) {
    StatusPayload *p = user_data;
    g_free(p->task->status_msg);
    p->task->status_msg = p->message;   /* take ownership */
    g_signal_emit(p->task, signals[SIGNAL_STATUS_CHANGED], 0, p->message);
    g_object_unref(p->task);
    g_free(p);   /* message now owned by task */
    return G_SOURCE_REMOVE;
}

void helvetia_task_report_progress(HelvetiaTask *self, double fraction) {
    ProgressPayload *p = g_new0(ProgressPayload, 1);
    p->task = g_object_ref(self);
    p->fraction = CLAMP(fraction, 0.0, 1.0);
    g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, emit_progress_idle, p, NULL);
}

void helvetia_task_set_status_message(HelvetiaTask *self, const char *message) {
    StatusPayload *p = g_new0(StatusPayload, 1);
    p->task = g_object_ref(self);
    p->message = g_strdup(message);
    g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, emit_status_idle, p, NULL);
}

/* ------------------------------------------------------------------ */
/* GTask plumbing                                                     */
/* ------------------------------------------------------------------ */

typedef struct {
    HelvetiaTask *task;
} TaskCallbackData;

static void task_callback_data_free(TaskCallbackData *d) {
    g_object_unref(d->task);
    g_free(d);
}

static void helvetia_task_on_gtask_done(GObject *source, GAsyncResult *result,
                                        gpointer user_data) {
    (void)source;
    TaskCallbackData *d = user_data;
    HelvetiaTask *self = d->task;

    GError *error = NULL;
    gboolean ok = g_task_propagate_boolean(G_TASK(result), &error);

    self->finished_at = g_get_monotonic_time();

    if (error) {
        if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
            self->status = HELVETIA_TASK_CANCELLED;
        } else {
            self->status = HELVETIA_TASK_FAILED;
            g_free(self->error);
            self->error = g_strdup(error->message);
        }
        g_error_free(error);
    } else if (ok) {
        self->status = HELVETIA_TASK_DONE;
    } else {
        self->status = HELVETIA_TASK_FAILED;
    }

    g_signal_emit(self, signals[SIGNAL_COMPLETED], 0,
                  self->status == HELVETIA_TASK_FAILED ? self->error : NULL);

    task_callback_data_free(d);
}

void helvetia_task_start(HelvetiaTask *self) {
    g_return_if_fail(HELVETIA_IS_TASK(self));
    g_return_if_fail(self->worker != NULL);
    g_return_if_fail(self->status == HELVETIA_TASK_PENDING);

    self->status = HELVETIA_TASK_RUNNING;
    self->started_at = g_get_monotonic_time();

    TaskCallbackData *d = g_new0(TaskCallbackData, 1);
    d->task = g_object_ref(self);

    self->gtask = g_task_new(NULL, self->cancellable,
                             helvetia_task_on_gtask_done, d);
    g_task_set_task_data(self->gtask, self->worker_data, self->worker_data_free);
    g_task_set_return_on_cancel(self->gtask, TRUE);
    g_task_run_in_thread(self->gtask, self->worker);
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

HelvetiaTask *helvetia_task_new(const char *name, const char *tool_id) {
    HelvetiaTask *self = g_object_new(HELVETIA_TYPE_TASK, NULL);
    self->name      = g_strdup(name);
    self->tool_id   = g_strdup(tool_id);
    self->status    = HELVETIA_TASK_PENDING;
    self->progress  = 0.0;
    self->cancellable = g_cancellable_new();
    return self;
}

void helvetia_task_set_worker(HelvetiaTask *self,
                              GTaskThreadFunc worker,
                              gpointer worker_data,
                              GDestroyNotify worker_data_free) {
    self->worker = worker;
    self->worker_data = worker_data;
    self->worker_data_free = worker_data_free;
}

void helvetia_task_cancel(HelvetiaTask *self) {
    if (self->status == HELVETIA_TASK_PENDING ||
        self->status == HELVETIA_TASK_RUNNING) {
        g_cancellable_cancel(self->cancellable);
    }
}

const char *helvetia_task_get_name(HelvetiaTask *self)        { return self->name; }
const char *helvetia_task_get_tool_id(HelvetiaTask *self)     { return self->tool_id; }
const char *helvetia_task_get_status_msg(HelvetiaTask *self)  { return self->status_msg; }
HelvetiaTaskStatus helvetia_task_get_status(HelvetiaTask *self) { return self->status; }
double helvetia_task_get_progress(HelvetiaTask *self)         { return self->progress; }
const char *helvetia_task_get_error(HelvetiaTask *self)       { return self->error; }
GCancellable *helvetia_task_get_cancellable(HelvetiaTask *self) { return self->cancellable; }

/* ------------------------------------------------------------------ */
/* GObject boilerplate                                                */
/* ------------------------------------------------------------------ */

static void helvetia_task_dispose(GObject *obj) {
    HelvetiaTask *self = HELVETIA_TASK(obj);

    if (self->gtask) {
        /* The GTask owns worker_data and frees it on finalization. */
        g_clear_object(&self->gtask);
    } else if (self->worker_data && self->worker_data_free) {
        /* The task never started; release the worker payload ourselves. */
        self->worker_data_free(self->worker_data);
    }
    self->worker_data = NULL;
    self->worker_data_free = NULL;

    g_clear_object(&self->cancellable);
    G_OBJECT_CLASS(helvetia_task_parent_class)->dispose(obj);
}

static void helvetia_task_finalize(GObject *obj) {
    HelvetiaTask *self = HELVETIA_TASK(obj);
    g_free(self->name);
    g_free(self->tool_id);
    g_free(self->status_msg);
    g_free(self->error);
    G_OBJECT_CLASS(helvetia_task_parent_class)->finalize(obj);
}

static void helvetia_task_class_init(HelvetiaTaskClass *klass) {
    GObjectClass *obj_class = G_OBJECT_CLASS(klass);
    obj_class->dispose  = helvetia_task_dispose;
    obj_class->finalize = helvetia_task_finalize;

    signals[SIGNAL_PROGRESS_CHANGED] =
        g_signal_new("progress-changed", HELVETIA_TYPE_TASK,
                     G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                     G_TYPE_NONE, 1, G_TYPE_DOUBLE);

    signals[SIGNAL_STATUS_CHANGED] =
        g_signal_new("status-changed", HELVETIA_TYPE_TASK,
                     G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                     G_TYPE_NONE, 1, G_TYPE_STRING);

    signals[SIGNAL_COMPLETED] =
        g_signal_new("completed", HELVETIA_TYPE_TASK,
                     G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                     G_TYPE_NONE, 1, G_TYPE_STRING);
}

static void helvetia_task_init(HelvetiaTask *self) {
    (void)self;
}