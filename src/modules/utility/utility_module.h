#pragma once
#include "../../core/plugin.h"
#include <gtk/gtk.h>

const HelvetiaModule *helvetia_utility_get_module(void);

/* Extra tool builders (utility_extra.c) */
GtkWidget *build_password_generator(void);
GtkWidget *build_word_counter       (void);
GtkWidget *build_regex_tester       (void);
GtkWidget *build_lorem_ipsum        (void);
GtkWidget *build_date_diff          (void);
GtkWidget *build_unix_timestamp     (void);
GtkWidget *build_text_diff          (void);
GtkWidget *build_stopwatch          (void);


GtkWidget *build_generic_text_tool(void);
GtkWidget *build_generic_math_tool(void);
GtkWidget *build_generic_random_tool(void);
GtkWidget *build_coming_soon(void);
GtkWidget *build_calc_sci(void);
GtkWidget *build_stddev_calc(void);
GtkWidget *build_pct_calc(void);
GtkWidget *build_vol_calc(void);
GtkWidget *build_frac_calc(void);
GtkWidget *build_tri_calc(void);
void format_scientific_unicode(double val, char *buf, size_t sz, int precision);
