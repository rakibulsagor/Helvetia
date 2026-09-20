#pragma once

#include "task.h"
#include <gio/gio.h>

G_BEGIN_DECLS

#define HELVETIA_TYPE_TASK_QUEUE (helvetia_task_queue_get_type())
G_DECLARE_FINAL_TYPE(HelvetiaTaskQueue, helvetia_task_queue, HELVETIA, TASK_QUEUE, GObject)

HelvetiaTaskQueue *helvetia_task_queue_new(void);

/* Get the process-wide queue, creating it on first call. */
HelvetiaTaskQueue *helvetia_task_queue_get_default(void);

/* Submit a task. The queue takes ownership of the reference. */
void helvetia_task_queue_submit(HelvetiaTaskQueue *self, HelvetiaTask *task);

/* Cancel every running task. */
void helvetia_task_queue_cancel_all(HelvetiaTaskQueue *self);

/* Remove completed tasks from the list. */
void helvetia_task_queue_clear_completed(HelvetiaTaskQueue *self);

/* Number of tasks currently running. */
guint helvetia_task_queue_active_count(HelvetiaTaskQueue *self);

/* GListModel of HelvetiaTask — for binding to UI. */
GListModel *helvetia_task_queue_get_model(HelvetiaTaskQueue *self);

G_END_DECLS