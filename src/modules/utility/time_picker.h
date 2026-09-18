#ifndef TIME_PICKER_H
#define TIME_PICKER_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

// Callback type when time is selected
// is_am is true if AM, false if PM
typedef void (*TimePickerCallback)(int hour, int minute, gboolean is_am, gpointer user_data);

// Shows a time picker dialog.
// parent: parent window (can be NULL)
// initial_hour: 0-23
// initial_minute: 0-59
// cb: callback when user clicks OK
// user_data: user data for callback
void show_time_picker(GtkWindow *parent,
                      int initial_hour,
                      int initial_minute,
                      TimePickerCallback cb,
                      gpointer user_data);

G_END_DECLS

#endif // TIME_PICKER_H
