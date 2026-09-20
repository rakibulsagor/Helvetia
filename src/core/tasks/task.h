#pragma once

#include <gio/gio.h>
#include <glib-object.h>

G_BEGIN_DECLS

typedef enum {
    HELVETIA_TASK_PENDING,
    HELVETIA_TASK_RUNNING,
    HELVETIA_TASK_DONE,
    HELVETIA_TASK_FAILED,
    HELVETIA_TASK_CANCELLED,
} HelvetiaTaskStatus;

#define HELVETIA_TYPE_TASK (helvetia_task_get_type())
G_DECLARE_FINAL_TYPE(HelvetiaTask, helvetia_task, HELVETIA, TASK, GObject)

/* Create a task with a human name and the tool that owns it. */
HelvetiaTask *helvetia_task_new(const char *name, const char *tool_id);

/* Set the worker. worker_data is freed by worker_data_free when the
   task is finalized. The worker runs on a background thread. */
void helvetia_task_set_worker(HelvetiaTask *self,
                              GTaskThreadFunc worker,
                              gpointer worker_data,
                              GDestroyNotify worker_data_free);

/* Start the task. Runs the worker on a GThread. */
void helvetia_task_start(HelvetiaTask *self);

/* Cancel the task. Cooperative: the worker must check the cancellable. */
void helvetia_task_cancel(HelvetiaTask *self);

/* Thread-safe: report progress from inside the worker. */
void helvetia_task_report_progress(HelvetiaTask *self, double fraction);

/* Thread-safe: report a status message ("Reading page 42 of 100…"). */
void helvetia_task_set_status_message(HelvetiaTask *self, const char *message);

/* Getters (main thread only) */
const char       *helvetia_task_get_name      (HelvetiaTask *self);
const char       *helvetia_task_get_tool_id   (HelvetiaTask *self);
const char       *helvetia_task_get_status_msg(HelvetiaTask *self);
HelvetiaTaskStatus helvetia_task_get_status   (HelvetiaTask *self);
double            helvetia_task_get_progress  (HelvetiaTask *self);
const char       *helvetia_task_get_error     (HelvetiaTask *self);
GCancellable     *helvetia_task_get_cancellable(HelvetiaTask *self);

G_END_DECLS