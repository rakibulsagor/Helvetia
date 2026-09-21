import re

with open('src/modules/image/tools/image_brightness_contrast.c', 'r') as f:
    code = f.read()

# 1. State
state_old = """    GtkWidget *save_btn;
} BCState;"""
state_new = """    GtkWidget *save_btn;

    GdkPixbuf *first_original;
    GPtrArray *undo_stack;
    GtkWidget *undo_btn;
} BCState;"""
code = code.replace(state_old, state_new)

# 2. Helpers
helpers = """
typedef struct {
    double brightness, contrast;
} BCSnapshot;

static BCSnapshot bc_snapshot(BCState *st) {
    return (BCSnapshot){ .brightness = st->brightness, .contrast = st->contrast };
}

static void bc_restore(BCState *st, BCSnapshot s) {
    st->brightness = s.brightness;
    st->contrast = s.contrast;

    gtk_range_set_value(GTK_RANGE(st->brightness_scale), s.brightness);
    gtk_range_set_value(GTK_RANGE(st->contrast_scale), s.contrast);

    char buf[32];
    snprintf(buf, sizeof buf, "%+.0f", s.brightness);
    gtk_label_set_text(GTK_LABEL(st->brightness_lbl), buf);

    snprintf(buf, sizeof buf, "%+.0f", s.contrast);
    gtk_label_set_text(GTK_LABEL(st->contrast_lbl), buf);

    regenerate(st);
}

static void push_snapshot(BCState *st) {
    BCSnapshot *s = g_new0(BCSnapshot, 1);
    *s = bc_snapshot(st);
    g_ptr_array_add(st->undo_stack, s);
    gtk_widget_set_sensitive(st->undo_btn, TRUE);
}

static void on_brightness_changed(GtkRange *r, gpointer d) {"""
code = code.replace('static void on_brightness_changed(GtkRange *r, gpointer d) {', helpers)

# 3. Callbacks
code = code.replace("""static void on_brightness_changed(GtkRange *r, gpointer d) {
    BCState *st = get_state(d);
    st->brightness = gtk_range_get_value(r);""", """static void on_brightness_changed(GtkRange *r, gpointer d) {
    BCState *st = get_state(d);
    double new_val = gtk_range_get_value(r);
    if (fabs(new_val - st->brightness) < 0.001) return;
    push_snapshot(st);
    st->brightness = new_val;""")

code = code.replace("""static void on_contrast_changed(GtkRange *r, gpointer d) {
    BCState *st = get_state(d);
    st->contrast = gtk_range_get_value(r);""", """static void on_contrast_changed(GtkRange *r, gpointer d) {
    BCState *st = get_state(d);
    double new_val = gtk_range_get_value(r);
    if (fabs(new_val - st->contrast) < 0.001) return;
    push_snapshot(st);
    st->contrast = new_val;""")

# 4. on_reset -> on_undo + on_reset
reset_old = """static void on_reset(GtkButton *b, gpointer d) {
    (void)b;
    BCState *st = get_state(d);
    st->brightness = 0;
    st->contrast = 0;
    gtk_range_set_value(GTK_RANGE(st->brightness_scale), 0);
    gtk_range_set_value(GTK_RANGE(st->contrast_scale), 0);
    gtk_label_set_text(GTK_LABEL(st->brightness_lbl), "+0");
    gtk_label_set_text(GTK_LABEL(st->contrast_lbl), "+0");
    regenerate(st);
}"""
reset_new = """static void on_undo(GtkButton *b, gpointer d) {
    (void)b;
    BCState *st = get_state(d);
    if (st->undo_stack->len == 0) return;

    BCSnapshot *s = g_ptr_array_index(st->undo_stack, st->undo_stack->len - 1);
    g_ptr_array_remove_index(st->undo_stack, st->undo_stack->len - 1);

    bc_restore(st, *s);
    g_free(s);

    gtk_widget_set_sensitive(st->undo_btn, st->undo_stack->len > 0);
}

static void on_reset(GtkButton *b, gpointer d) {
    (void)b;
    BCState *st = get_state(d);
    if (!st->original) return;

    push_snapshot(st);

    st->brightness = 0;
    st->contrast = 0;
    gtk_range_set_value(GTK_RANGE(st->brightness_scale), 0);
    gtk_range_set_value(GTK_RANGE(st->contrast_scale), 0);
    gtk_label_set_text(GTK_LABEL(st->brightness_lbl), "+0");
    gtk_label_set_text(GTK_LABEL(st->contrast_lbl), "+0");
    regenerate(st);
}"""
code = code.replace(reset_old, reset_new)

# 5. on_drop
drop_old = """    g_clear_object(&st->original);
    g_clear_object(&st->preview);
    g_free(st->path);

    st->original = pb;
    st->path = g_strdup(path);"""
drop_new = """    g_clear_object(&st->original);
    g_clear_object(&st->preview);
    g_clear_object(&st->first_original);
    g_free(st->path);

    st->original = pb;
    st->first_original = g_object_ref(pb);
    st->path = g_strdup(path);

    if (st->undo_stack) {
        for (guint i = 0; i < st->undo_stack->len; i++)
            g_free(g_ptr_array_index(st->undo_stack, i));
        g_ptr_array_set_size(st->undo_stack, 0);
    }
    if (st->undo_btn) gtk_widget_set_sensitive(st->undo_btn, FALSE);"""
code = code.replace(drop_old, drop_new)

# 6. lifecycle
lifecycle_old = """void image_brightness_contrast_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "bc-state", NULL);
}"""
lifecycle_new = """static void bc_state_free(BCState *st) {
    if (!st) return;
    g_clear_object(&st->original);
    g_clear_object(&st->preview);
    g_clear_object(&st->first_original);
    if (st->undo_stack) {
        for (guint i = 0; i < st->undo_stack->len; i++)
            g_free(g_ptr_array_index(st->undo_stack, i));
        g_ptr_array_unref(st->undo_stack);
    }
    g_free(st->path);
    g_free(st);
}

void image_brightness_contrast_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "bc-state", NULL);
}"""
code = code.replace(lifecycle_old, lifecycle_new)
code = code.replace('g_object_set_data_full(G_OBJECT(root), "bc-state", st, g_free);', 'g_object_set_data_full(G_OBJECT(root), "bc-state", st, (GDestroyNotify)bc_state_free);')

# 7. create
code = code.replace('    BCState *st = g_new0(BCState, 1);\n\n    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);', '    BCState *st = g_new0(BCState, 1);\n    st->undo_stack = g_ptr_array_new();\n\n    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);')

btn_old = """    st->reset_btn = gtk_button_new_with_label("Reset");
    gtk_widget_add_css_class(st->reset_btn, "flat");

    st->save_btn = gtk_button_new_with_label("Save As…");
    gtk_widget_add_css_class(st->save_btn, "flat");

    /* Compose toolbar: two rows to fit narrow windows */
    GtkWidget *bar_row1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_append(GTK_BOX(bar_row1), br_lbl);
    gtk_box_append(GTK_BOX(bar_row1), st->brightness_scale);
    gtk_box_append(GTK_BOX(bar_row1), st->brightness_lbl);
    gtk_box_append(GTK_BOX(bar_row1), gtk_separator_new(GTK_ORIENTATION_VERTICAL));
    gtk_box_append(GTK_BOX(bar_row1), ct_lbl);
    gtk_box_append(GTK_BOX(bar_row1), st->contrast_scale);
    gtk_box_append(GTK_BOX(bar_row1), st->contrast_lbl);

    gtk_box_append(GTK_BOX(bar), bar_row1);
    gtk_box_append(GTK_BOX(bar), image_new_image_button(on_drop, root));
    gtk_box_append(GTK_BOX(bar), sp);
    gtk_box_append(GTK_BOX(bar), st->reset_btn);
    gtk_box_append(GTK_BOX(bar), st->save_btn);"""

btn_new = """    st->undo_btn = image_undo_button(G_CALLBACK(on_undo), root);
    gtk_widget_set_sensitive(st->undo_btn, FALSE);

    st->reset_btn = image_reset_button(G_CALLBACK(on_reset), root);

    st->save_btn = gtk_button_new_with_label("Save As…");
    gtk_widget_add_css_class(st->save_btn, "flat");

    /* Compose toolbar: two rows to fit narrow windows */
    GtkWidget *bar_row1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_append(GTK_BOX(bar_row1), br_lbl);
    gtk_box_append(GTK_BOX(bar_row1), st->brightness_scale);
    gtk_box_append(GTK_BOX(bar_row1), st->brightness_lbl);
    gtk_box_append(GTK_BOX(bar_row1), gtk_separator_new(GTK_ORIENTATION_VERTICAL));
    gtk_box_append(GTK_BOX(bar_row1), ct_lbl);
    gtk_box_append(GTK_BOX(bar_row1), st->contrast_scale);
    gtk_box_append(GTK_BOX(bar_row1), st->contrast_lbl);

    gtk_box_append(GTK_BOX(bar), bar_row1);
    gtk_box_append(GTK_BOX(bar), sp);
    gtk_box_append(GTK_BOX(bar), image_new_image_button(on_drop, root));
    gtk_box_append(GTK_BOX(bar), st->undo_btn);
    gtk_box_append(GTK_BOX(bar), st->reset_btn);
    gtk_box_append(GTK_BOX(bar), st->save_btn);"""
code = code.replace(btn_old, btn_new)
code = code.replace('g_signal_connect(st->reset_btn, "clicked", G_CALLBACK(on_reset), root);', '')

with open('src/modules/image/tools/image_brightness_contrast.c', 'w') as f:
    f.write(code)

