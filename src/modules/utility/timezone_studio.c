#include "timezone_studio.h"
#include "time_picker.h"
#include <string.h>
#include <math.h>

static const char *TIMEZONES[] = {"Local Time",
                                  "UTC",
                                  "America/New_York",
                                  "America/Los_Angeles",
                                  "America/Chicago",
                                  "America/Denver",
                                  "America/Toronto",
                                  "America/Mexico_City",
                                  "America/Sao_Paulo",
                                  "America/Buenos_Aires",
                                  "America/Anchorage",
                                  "America/Caracas",
                                  "America/Lima",
                                  "America/Bogota",
                                  "America/Halifax",
                                  "Europe/London",
                                  "Europe/Paris",
                                  "Europe/Berlin",
                                  "Europe/Rome",
                                  "Europe/Madrid",
                                  "Europe/Moscow",
                                  "Europe/Istanbul",
                                  "Europe/Amsterdam",
                                  "Europe/Athens",
                                  "Europe/Brussels",
                                  "Europe/Dublin",
                                  "Europe/Helsinki",
                                  "Europe/Kiev",
                                  "Europe/Oslo",
                                  "Europe/Prague",
                                  "Europe/Stockholm",
                                  "Europe/Vienna",
                                  "Europe/Warsaw",
                                  "Europe/Zurich",
                                  "Asia/Tokyo",
                                  "Asia/Shanghai",
                                  "Asia/Hong_Kong",
                                  "Asia/Seoul",
                                  "Asia/Dubai",
                                  "Asia/Kolkata",
                                  "Asia/Singapore",
                                  "Asia/Bangkok",
                                  "Asia/Jakarta",
                                  "Asia/Taipei",
                                  "Asia/Manila",
                                  "Asia/Ho_Chi_Minh",
                                  "Asia/Kuala_Lumpur",
                                  "Asia/Dhaka",
                                  "Asia/Karachi",
                                  "Asia/Riyadh",
                                  "Asia/Tehran",
                                  "Australia/Sydney",
                                  "Australia/Melbourne",
                                  "Australia/Brisbane",
                                  "Australia/Perth",
                                  "Australia/Adelaide",
                                  "Australia/Darwin",
                                  "Australia/Hobart",
                                  "Pacific/Auckland",
                                  "Pacific/Honolulu",
                                  "Pacific/Niue",
                                  "Pacific/Fiji",
                                  "Pacific/Pago_Pago",
                                  "Pacific/Guam",
                                  "Pacific/Port_Moresby",
                                  "Africa/Cairo",
                                  "Africa/Johannesburg",
                                  "Africa/Lagos",
                                  "Africa/Nairobi",
                                  "Africa/Casablanca",
                                  "Africa/Addis_Ababa",
                                  "Etc/GMT+12",
                                  "Etc/GMT+11",
                                  "Etc/GMT+10",
                                  "Etc/GMT+9",
                                  "Etc/GMT+8",
                                  "Etc/GMT+7",
                                  "Etc/GMT+6",
                                  "Etc/GMT+5",
                                  "Etc/GMT+4",
                                  "Etc/GMT+3",
                                  "Etc/GMT+2",
                                  "Etc/GMT+1",
                                  "Etc/GMT-1",
                                  "Etc/GMT-12",
                                  "Etc/GMT-11",
                                  "Etc/GMT-10",
                                  "Etc/GMT-9",
                                  "Etc/GMT-8",
                                  "Etc/GMT-7",
                                  "Etc/GMT-6",
                                  "Etc/GMT-5",
                                  "Etc/GMT-4",
                                  "Etc/GMT-3",
                                  "Etc/GMT-2",
                                  NULL};

typedef struct {
  GtkWidget *list_box;
  GtkWidget *search_combo;
  GtkWidget *slider;
  GtkWidget *time_entry;
  GtkWidget *btn_12h;
  GtkWidget *btn_24h;
  GtkWidget *date_btn;

  gboolean is_24h;
  GDateTime *base_midnight;
} TzState;

static void update_all_times(TzState *state);
static void add_timezone_row(TzState *state, const char *tz_id);

static void on_world_clock_view(GtkButton *btn, gpointer data) {
    (void)btn;
  TzState *state = data;

  // Remove all existing rows
  GtkWidget *child = gtk_widget_get_first_child(state->list_box);
  while (child) {
    GtkWidget *next = gtk_widget_get_next_sibling(child);
    gtk_list_box_remove(GTK_LIST_BOX(state->list_box), child);
    child = next;
  }

  // Add world clock defaults
  add_timezone_row(state, "Local Time");
  add_timezone_row(state, "America/New_York");
  add_timezone_row(state, "Europe/London");
  add_timezone_row(state, "Asia/Dubai");
  add_timezone_row(state, "Asia/Tokyo");
  add_timezone_row(state, "Australia/Sydney");
}

static GDateTime *get_local_midnight(void) {
  GDateTime *now = g_date_time_new_now_local();
  int y = g_date_time_get_year(now);
  int m = g_date_time_get_month(now);
  int d = g_date_time_get_day_of_month(now);
  GTimeZone *tz = g_time_zone_new_local();
  GDateTime *mid = g_date_time_new(tz, y, m, d, 0, 0, 0);
  g_time_zone_unref(tz);
  g_date_time_unref(now);
  return mid;
}

static void on_time_entry_activated(GtkEntry *entry, gpointer data) {
  (void)entry;
  TzState *state = (TzState *)data;
  const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
  int h = 0, m = 0;
  char ampm[8] = {0};
  
  if (sscanf(text, "%d:%d %7s", &h, &m, ampm) >= 2) {
      if (g_ascii_strcasecmp(ampm, "PM") == 0 && h < 12) h += 12;
      else if (g_ascii_strcasecmp(ampm, "AM") == 0 && h == 12) h = 0;
      double seconds = h * 3600.0 + m * 60.0;
      gtk_range_set_value(GTK_RANGE(state->slider), seconds);
  } else if (sscanf(text, "%d %7s", &h, ampm) >= 1) {
      if (g_ascii_strcasecmp(ampm, "PM") == 0 && h < 12) h += 12;
      else if (g_ascii_strcasecmp(ampm, "AM") == 0 && h == 12) h = 0;
      double seconds = h * 3600.0;
      gtk_range_set_value(GTK_RANGE(state->slider), seconds);
  }
}

static void on_time_picked(int hour, int minute, gboolean is_am, gpointer data) {
    TzState *state = (TzState *)data;
    if (is_am && hour == 12) hour = 0;
    else if (!is_am && hour < 12) hour += 12;
    double seconds = hour * 3600.0 + minute * 60.0;
    gtk_range_set_value(GTK_RANGE(state->slider), seconds);
}

static void on_clock_btn_clicked(GtkButton *btn, gpointer data) {
    (void)btn;
    TzState *state = (TzState *)data;
    GtkWidget *win = GTK_WIDGET(gtk_widget_get_root(state->slider));
    
    double seconds = gtk_range_get_value(GTK_RANGE(state->slider));
    int h = (int)(seconds / 3600.0);
    int m = (int)(fmod(seconds, 3600.0) / 60.0);
    
    show_time_picker(GTK_WINDOW(win), h, m, on_time_picked, state);
}

static void on_slider_changed(GtkRange *range, gpointer data) {
  (void)range;
  TzState *state = (TzState *)data;
  update_all_times(state);
}

static void on_format_toggled(GtkToggleButton *btn, gpointer data) {
  (void)btn;
  TzState *state = (TzState *)data;
  state->is_24h =
      gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(state->btn_24h));
  update_all_times(state);
}

static void on_reset_clicked(GtkButton *btn, gpointer data) {
  (void)btn;
  TzState *state = (TzState *)data;
  GDateTime *now = g_date_time_new_now_local();
  double offset_sec = g_date_time_get_hour(now) * 3600.0 +
                      g_date_time_get_minute(now) * 60.0 +
                      g_date_time_get_second(now);
  g_date_time_unref(now);
  gtk_range_set_value(GTK_RANGE(state->slider), offset_sec);
}

static void on_remove_row(GtkButton *btn, gpointer data) {
  (void)btn;
  GtkWidget *row = GTK_WIDGET(data);
  GtkWidget *list = gtk_widget_get_parent(row);
  if (list) {
    gtk_list_box_remove(GTK_LIST_BOX(list), row);
  }
}

static void format_tz_name(const char *id, char *out_city, char *out_region) {
  if (strcmp(id, "Local Time") == 0 || strcmp(id, "UTC") == 0) {
    strcpy(out_city, id);
    strcpy(out_region, "");
    return;
  }

  char tmp[64];
  strncpy(tmp, id, sizeof(tmp) - 1);
  tmp[sizeof(tmp) - 1] = '\0';

  char *slash = strchr(tmp, '/');
  if (slash) {
    *slash = '\0';
    char *city = slash + 1;
    // replace underscore with space
    for (char *c = city; *c; c++) {
      if (*c == '_')
        *c = ' ';
    }
    strcpy(out_city, city);
    strcpy(out_region, tmp);
  } else {
    strcpy(out_city, tmp);
    strcpy(out_region, "");
  }
}

static void update_row(GtkWidget *row, GDateTime *global_time,
                       gboolean is_24h) {
  const char *tz_id = g_object_get_data(G_OBJECT(row), "tz_id");
  if (!tz_id)
    return;

  GTimeZone *tz = NULL;
  if (strcmp(tz_id, "Local Time") == 0) {
    tz = g_time_zone_new_local();
  } else if (strcmp(tz_id, "UTC") == 0) {
    tz = g_time_zone_new_utc();
  } else {
    tz = g_time_zone_new_identifier(tz_id);
    if (!tz)
      tz = g_time_zone_new_utc(); // fallback
  }

  GDateTime *local_dt = g_date_time_to_timezone(global_time, tz);

  char *time_str;
  if (is_24h) {
    time_str = g_date_time_format(local_dt, "%H:%M:%S");
  } else {
    time_str = g_date_time_format(local_dt, "%I:%M:%S %p");
  }

  // offset string
  GTimeSpan offset = g_date_time_get_utc_offset(local_dt);
  int hours = offset / G_TIME_SPAN_HOUR;
  int mins = (offset % G_TIME_SPAN_HOUR) / G_TIME_SPAN_MINUTE;
  char offset_str[32];
  if (mins == 0) {
    snprintf(offset_str, sizeof(offset_str), "GMT%+d", hours);
  } else {
    snprintf(offset_str, sizeof(offset_str), "GMT%+d:%02d", hours,
             mins > 0 ? mins : -mins);
  }

  char *date_str = g_date_time_format(local_dt, "%a, %b %d");

  GtkWidget *time_lbl = g_object_get_data(G_OBJECT(row), "time_lbl");
  GtkWidget *date_lbl = g_object_get_data(G_OBJECT(row), "date_lbl");
  GtkWidget *offset_lbl = g_object_get_data(G_OBJECT(row), "offset_lbl");

  if (time_lbl)
    gtk_label_set_text(GTK_LABEL(time_lbl), time_str);
  if (date_lbl)
    gtk_label_set_text(GTK_LABEL(date_lbl), date_str);
  if (offset_lbl)
    gtk_label_set_text(GTK_LABEL(offset_lbl), offset_str);

  g_free(time_str);
  g_free(date_str);
  g_date_time_unref(local_dt);
  g_time_zone_unref(tz);
}

static void update_all_times(TzState *state) {
  if (!state->base_midnight)
    return;

  double offset_sec = gtk_range_get_value(GTK_RANGE(state->slider));
  GDateTime *current_global =
      g_date_time_add_seconds(state->base_midnight, offset_sec);

  // Update header date
  char *date_str = g_date_time_format(current_global, "%m / %d / %Y");
  gtk_button_set_label(GTK_BUTTON(state->date_btn), date_str);
  g_free(date_str);
  
  if (state->time_entry && !gtk_widget_has_focus(state->time_entry)) {
      char *time_str;
      if (state->is_24h) {
          time_str = g_date_time_format(current_global, "%H:%M");
      } else {
          time_str = g_date_time_format(current_global, "%I:%M %p");
      }
      if (time_str) {
          gtk_editable_set_text(GTK_EDITABLE(state->time_entry), time_str);
          g_free(time_str);
      }
  }

  GtkWidget *child = gtk_widget_get_first_child(state->list_box);
  while (child) {
    update_row(child, current_global, state->is_24h);
    child = gtk_widget_get_next_sibling(child);
  }

  g_date_time_unref(current_global);
}

static void add_timezone_row(TzState *state, const char *tz_id) {
  GtkWidget *row = gtk_list_box_row_new();
  gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(row), FALSE);
  g_object_set_data_full(G_OBJECT(row), "tz_id", g_strdup(tz_id), g_free);

  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_set_margin_top(box, 16);
  gtk_widget_set_margin_bottom(box, 16);
  gtk_widget_set_margin_start(box, 16);
  gtk_widget_set_margin_end(box, 16);

  // Decorator line
  GtkWidget *dec = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
  gtk_widget_add_css_class(dec, "accent");
  gtk_widget_set_size_request(dec, 4, -1);
  gtk_box_append(GTK_BOX(box), dec);

  // Left: City / Region
  GtkWidget *left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
  char city[64], region[64];
  format_tz_name(tz_id, city, region);

  GtkWidget *city_lbl = gtk_label_new(city);
  gtk_widget_add_css_class(city_lbl, "title-2");
  gtk_widget_set_halign(city_lbl, GTK_ALIGN_START);

  GtkWidget *reg_lbl = gtk_label_new(region);
  gtk_widget_add_css_class(reg_lbl, "dim-label");
  gtk_widget_set_halign(reg_lbl, GTK_ALIGN_START);

  gtk_box_append(GTK_BOX(left), city_lbl);
  gtk_box_append(GTK_BOX(left), reg_lbl);
  gtk_widget_set_hexpand(left, TRUE);
  gtk_box_append(GTK_BOX(box), left);

  // Right: Time / Offset / Date
  GtkWidget *right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
  gtk_widget_set_halign(right, GTK_ALIGN_END);

  GtkWidget *time_lbl = gtk_label_new("00:00:00");
  gtk_widget_add_css_class(time_lbl, "title-2");

  GtkWidget *details_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  GtkWidget *offset_lbl = gtk_label_new("UTC+0");
  gtk_widget_add_css_class(offset_lbl, "dim-label");
  GtkWidget *date_lbl = gtk_label_new("Mon, Jan 01");
  gtk_widget_add_css_class(date_lbl, "dim-label");
  gtk_box_append(GTK_BOX(details_box), date_lbl);
  gtk_box_append(GTK_BOX(details_box),
                 gtk_separator_new(GTK_ORIENTATION_VERTICAL));
  gtk_box_append(GTK_BOX(details_box), offset_lbl);

  gtk_box_append(GTK_BOX(right), time_lbl);
  gtk_box_append(GTK_BOX(right), details_box);

  g_object_set_data(G_OBJECT(row), "time_lbl", time_lbl);
  g_object_set_data(G_OBJECT(row), "date_lbl", date_lbl);
  g_object_set_data(G_OBJECT(row), "offset_lbl", offset_lbl);

  gtk_box_append(GTK_BOX(box), right);

  GtkWidget *del_btn = gtk_button_new_from_icon_name("window-close-symbolic");
  gtk_widget_add_css_class(del_btn, "circular");
  gtk_widget_add_css_class(del_btn, "flat");
  gtk_widget_set_valign(del_btn, GTK_ALIGN_CENTER);
  g_signal_connect(del_btn, "clicked", G_CALLBACK(on_remove_row), row);

  // don't allow deleting local time if it's the first
  if (strcmp(tz_id, "Local Time") != 0) {
    gtk_box_append(GTK_BOX(box), del_btn);
  } else {
    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_size_request(spacer, 32, -1);
    gtk_box_append(GTK_BOX(box), spacer);
  }

  gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
  gtk_list_box_append(GTK_LIST_BOX(state->list_box), row);

  if (state->base_midnight) {
    double offset_sec = gtk_range_get_value(GTK_RANGE(state->slider));
    GDateTime *current_global =
        g_date_time_add_seconds(state->base_midnight, offset_sec);
    update_row(row, current_global, state->is_24h);
    g_date_time_unref(current_global);
  }
}

static void on_add_btn_clicked(GtkButton *btn, gpointer data) {
  (void)btn;
  TzState *state = (TzState *)data;
  GObject *item = gtk_drop_down_get_selected_item(GTK_DROP_DOWN(state->search_combo));
  if (!item || !GTK_IS_STRING_OBJECT(item))
    return;

  const char *full_str = gtk_string_object_get_string(GTK_STRING_OBJECT(item));
  if (!full_str)
    return;

  if (strcmp(full_str, "Local Time") == 0 || strcmp(full_str, "UTC") == 0) {
    add_timezone_row(state, full_str);
    return;
  }

  char tz_id[128];
  strncpy(tz_id, full_str, sizeof(tz_id) - 1);
  tz_id[sizeof(tz_id) - 1] = '\0';
  char *space = strchr(tz_id, ' ');
  if (space)
    *space = '\0';

  add_timezone_row(state, tz_id);
}

static void on_state_destroy(gpointer data) {
  TzState *state = data;
  if (state->base_midnight) {
    g_date_time_unref(state->base_midnight);
  }
  g_free(state);
}

GtkWidget *build_timezone_studio(void) {
  TzState *state = g_new0(TzState, 1);
  state->is_24h = FALSE;
  state->base_midnight = get_local_midnight();

  GtkWidget *main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 24);
  gtk_widget_set_margin_top(main_vbox, 32);
  gtk_widget_set_margin_bottom(main_vbox, 32);
  gtk_widget_set_margin_start(main_vbox, 32);
  gtk_widget_set_margin_end(main_vbox, 32);
  g_signal_connect_swapped(main_vbox, "destroy", G_CALLBACK(on_state_destroy),
                           state);

  GtkWidget *title = gtk_label_new("Time Zone Converter");
  gtk_widget_add_css_class(title, "title-1");
  gtk_widget_set_halign(title, GTK_ALIGN_CENTER);
  gtk_box_append(GTK_BOX(main_vbox), title);

  // Top Controls Card
  GtkWidget *ctrl_frame = gtk_frame_new(NULL);
  GtkWidget *ctrl_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
  gtk_widget_set_margin_top(ctrl_box, 16);
  gtk_widget_set_margin_bottom(ctrl_box, 16);
  gtk_widget_set_margin_start(ctrl_box, 16);
  gtk_widget_set_margin_end(ctrl_box, 16);
  gtk_frame_set_child(GTK_FRAME(ctrl_frame), ctrl_box);

  // Search row
  GtkWidget *search_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  GtkWidget *search_icon =
      gtk_image_new_from_icon_name("system-search-symbolic");

  GtkStringList *string_list = gtk_string_list_new(NULL);
  for (int i = 0; TIMEZONES[i] != NULL; i++) {
    const char *id = TIMEZONES[i];
    if (strcmp(id, "Local Time") == 0 || strcmp(id, "UTC") == 0) {
      gtk_string_list_append(string_list, id);
      continue;
    }

    GTimeZone *tz = g_time_zone_new_identifier(id);
    if (tz) {
      GDateTime *dt = g_date_time_new_now(tz);
      GTimeSpan offset = g_date_time_get_utc_offset(dt);
      int hours = offset / G_TIME_SPAN_HOUR;
      int mins = (offset % G_TIME_SPAN_HOUR) / G_TIME_SPAN_MINUTE;
      char buf[128];
      if (mins == 0)
        snprintf(buf, sizeof(buf), "%s (GMT%+d)", id, hours);
      else
        snprintf(buf, sizeof(buf), "%s (GMT%+d:%02d)", id, hours,
                 mins > 0 ? mins : -mins);

      gtk_string_list_append(string_list, buf);

      g_date_time_unref(dt);
      g_time_zone_unref(tz);
    } else {
      gtk_string_list_append(string_list, id);
    }
  }

  state->search_combo = gtk_drop_down_new(G_LIST_MODEL(string_list), NULL);
  g_object_unref(string_list);

  gtk_drop_down_set_enable_search(GTK_DROP_DOWN(state->search_combo), TRUE);
  GtkExpression *expr =
      gtk_property_expression_new(GTK_TYPE_STRING_OBJECT, NULL, "string");
  gtk_drop_down_set_expression(GTK_DROP_DOWN(state->search_combo), expr);
  gtk_expression_unref(expr);

  gtk_widget_set_hexpand(state->search_combo, TRUE);

  GtkWidget *add_btn = gtk_button_new_from_icon_name("list-add-symbolic");
  gtk_widget_set_tooltip_text(add_btn, "Add Selected Timezone");
  g_signal_connect(add_btn, "clicked", G_CALLBACK(on_add_btn_clicked), state);

  gtk_box_append(GTK_BOX(search_box), search_icon);
  gtk_box_append(GTK_BOX(search_box), state->search_combo);
  gtk_box_append(GTK_BOX(search_box), add_btn);
  gtk_box_append(GTK_BOX(ctrl_box), search_box);

  // Format row
  GtkWidget *fmt_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  GtkWidget *cal_icon =
      gtk_image_new_from_icon_name("x-office-calendar-symbolic");
  state->date_btn = gtk_button_new_with_label("01 / 01 / 2026"); // dynamic
  gtk_widget_add_css_class(state->date_btn, "flat");

  GtkWidget *clock_icon =
      gtk_image_new_from_icon_name("preferences-system-time-symbolic");
  GtkWidget *fmt_grp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_add_css_class(fmt_grp, "linked");
  state->btn_12h = gtk_toggle_button_new_with_label("12H");
  state->btn_24h = gtk_toggle_button_new_with_label("24H");
  gtk_toggle_button_set_group(GTK_TOGGLE_BUTTON(state->btn_24h),
                              GTK_TOGGLE_BUTTON(state->btn_12h));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(state->btn_12h), TRUE);
  g_signal_connect(state->btn_12h, "toggled", G_CALLBACK(on_format_toggled),
                   state);
  g_signal_connect(state->btn_24h, "toggled", G_CALLBACK(on_format_toggled),
                   state);
  gtk_box_append(GTK_BOX(fmt_grp), state->btn_12h);
  gtk_box_append(GTK_BOX(fmt_grp), state->btn_24h);

  GtkWidget *reset_btn = gtk_button_new_with_label("Reset");
  GtkWidget *reset_icon = gtk_image_new_from_icon_name("edit-undo-symbolic");
  gtk_button_set_child(GTK_BUTTON(reset_btn),
                       gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4));
  gtk_box_append(GTK_BOX(gtk_button_get_child(GTK_BUTTON(reset_btn))),
                 reset_icon);
  gtk_box_append(GTK_BOX(gtk_button_get_child(GTK_BUTTON(reset_btn))),
                 gtk_label_new("Reset"));
  g_signal_connect(reset_btn, "clicked", G_CALLBACK(on_reset_clicked), state);

  gtk_box_append(GTK_BOX(fmt_box), cal_icon);
  gtk_box_append(GTK_BOX(fmt_box), state->date_btn);
  GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_hexpand(spacer, TRUE);
  gtk_box_append(GTK_BOX(fmt_box), spacer);
  gtk_box_append(GTK_BOX(fmt_box), clock_icon);
  gtk_box_append(GTK_BOX(fmt_box), fmt_grp);
  gtk_box_append(GTK_BOX(fmt_box), reset_btn);
  gtk_box_append(GTK_BOX(ctrl_box), fmt_box);

  gtk_box_append(GTK_BOX(main_vbox), ctrl_frame);

  // List Card
  GtkWidget *list_frame = gtk_frame_new(NULL);
  GtkWidget *list_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  state->list_box = gtk_list_box_new();
  gtk_widget_add_css_class(state->list_box, "rich-list");
  gtk_box_append(GTK_BOX(list_vbox), state->list_box);

  // Slider
  GtkWidget *slider_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
  gtk_widget_set_margin_top(slider_box, 16);
  gtk_widget_set_margin_bottom(slider_box, 16);
  gtk_widget_set_margin_start(slider_box, 24);
  gtk_widget_set_margin_end(slider_box, 24);

  GtkWidget *input_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_halign(input_box, GTK_ALIGN_CENTER);
  state->time_entry = gtk_entry_new();
  gtk_entry_set_placeholder_text(GTK_ENTRY(state->time_entry), "e.g., 14:30 or 2:30 PM (Press Enter)");
  gtk_widget_set_size_request(state->time_entry, 240, -1);
  g_signal_connect(state->time_entry, "activate", G_CALLBACK(on_time_entry_activated), state);
  
  GtkWidget *clock_btn = gtk_button_new_from_icon_name("preferences-system-time-symbolic");
  gtk_widget_set_tooltip_text(clock_btn, "Open Analog Clock");
  g_signal_connect(clock_btn, "clicked", G_CALLBACK(on_clock_btn_clicked), state);

  gtk_box_append(GTK_BOX(input_box), gtk_label_new("Set Local Time:"));
  gtk_box_append(GTK_BOX(input_box), state->time_entry);
  gtk_box_append(GTK_BOX(input_box), clock_btn);
  gtk_box_append(GTK_BOX(slider_box), input_box);

  GtkWidget *marks_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_append(GTK_BOX(marks_box), gtk_label_new("12AM"));
  GtkWidget *sp1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_hexpand(sp1, TRUE);
  gtk_box_append(GTK_BOX(marks_box), sp1);
  gtk_box_append(GTK_BOX(marks_box), gtk_label_new("6AM"));
  GtkWidget *sp2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_hexpand(sp2, TRUE);
  gtk_box_append(GTK_BOX(marks_box), sp2);
  gtk_box_append(GTK_BOX(marks_box), gtk_label_new("12PM"));
  GtkWidget *sp3 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_hexpand(sp3, TRUE);
  gtk_box_append(GTK_BOX(marks_box), sp3);
  gtk_box_append(GTK_BOX(marks_box), gtk_label_new("6PM"));
  GtkWidget *sp4 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_hexpand(sp4, TRUE);
  gtk_box_append(GTK_BOX(marks_box), sp4);
  gtk_box_append(GTK_BOX(marks_box), gtk_label_new("12AM"));
  gtk_box_append(GTK_BOX(slider_box), marks_box);

  state->slider =
      gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 86400.0, 60.0);
  gtk_widget_set_hexpand(state->slider, TRUE);
  gtk_scale_set_draw_value(GTK_SCALE(state->slider), FALSE);
  g_signal_connect(state->slider, "value-changed",
                   G_CALLBACK(on_slider_changed), state);
  gtk_box_append(GTK_BOX(slider_box), state->slider);

  GtkWidget *slider_hint =
      gtk_label_new("Drag slider to adjust time • All timezones update");
  gtk_widget_add_css_class(slider_hint, "dim-label");
  gtk_box_append(GTK_BOX(slider_box), slider_hint);

  gtk_box_append(GTK_BOX(list_vbox), slider_box);
  gtk_frame_set_child(GTK_FRAME(list_frame), list_vbox);

  gtk_box_append(GTK_BOX(main_vbox), list_frame);

  // Quick Actions
  GtkWidget *action_frame = gtk_frame_new(NULL);
  GtkWidget *action_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
  gtk_widget_set_margin_top(action_box, 16);
  gtk_widget_set_margin_bottom(action_box, 16);
  gtk_widget_set_margin_start(action_box, 16);
  gtk_widget_set_margin_end(action_box, 16);

  GtkWidget *action_title = gtk_label_new("Quick Actions");
  gtk_widget_add_css_class(action_title, "heading");
  gtk_widget_set_halign(action_title, GTK_ALIGN_START);
  gtk_box_append(GTK_BOX(action_box), action_title);

  GtkWidget *grid = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  GtkWidget *btn_copy = gtk_button_new_with_label("Copy Times");
  GtkWidget *btn_world_clock = gtk_button_new_with_label("World Clock View");

  g_signal_connect(btn_world_clock, "clicked", G_CALLBACK(on_world_clock_view),
                   state);

  gtk_box_append(GTK_BOX(grid), btn_copy);
  gtk_box_append(GTK_BOX(grid), btn_world_clock);
  gtk_box_append(GTK_BOX(action_box), grid);

  gtk_frame_set_child(GTK_FRAME(action_frame), action_box);
  gtk_box_append(GTK_BOX(main_vbox), action_frame);

  // Initial Setup
  add_timezone_row(state, "Local Time");
  on_reset_clicked(NULL, state);

  return main_vbox;
}
