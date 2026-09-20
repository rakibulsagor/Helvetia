#pragma once

#include <adwaita.h>
#include "../core/tasks/task_queue.h"

G_BEGIN_DECLS

#define HELVETIA_TYPE_TASK_STRIP (helvetia_task_strip_get_type())
G_DECLARE_FINAL_TYPE(HelvetiaTaskStrip, helvetia_task_strip, HELVETIA, TASK_STRIP, GtkBox)

HelvetiaTaskStrip *helvetia_task_strip_new(HelvetiaTaskQueue *queue);

G_END_DECLS