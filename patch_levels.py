import re

with open('src/modules/image/tools/image_levels.c', 'r') as f:
    code = f.read()

# 1. LevelsState
code = code.replace(
    '    GtkWidget *out_white_scale,*out_white_lbl;\n}',
    '    GtkWidget *out_white_scale,*out_white_lbl;\n\n    GdkPixbuf *first_original;\n    GPtrArray *undo_stack;\n    GtkWidget *undo_btn;\n}'
)

# 2. Helpers before on_in_black
helpers = """
typedef struct {
    int    in_black, in_white;
    double gamma;
    int    out_black, out_white;
} LevelsSnapshot;

static LevelsSnapshot levels_snapshot(LevelsState *st) {
    return (LevelsSnapshot){
        .in_black = st->in_black,
        .in_white = st->in_white,
        .gamma = st->gamma,
        .out_black = st->out_black,
        .out_white = st->out_white,
    };
}

static void levels_restore(LevelsState *st, LevelsSnapshot s) {
    st->in_black = s.in_black;
    st->in_white = s.in_white;
    st->gamma = s.gamma;
    st->out_black = s.out_black;
    st->out_white = s.out_white;

    gtk_range_set_value(GTK_RANGE(st->in_black_scale), s.in_black);
    gtk_range_set_value(GTK_RANGE(st->in_white_scale), s.in_white);
    gtk_range_set_value(GTK_RANGE(st->gamma_scale), s.gamma);
    gtk_range_set_value(GTK_RANGE(st->out_black_scale), s.out_black);
    gtk_range_set_value(GTK_RANGE(st->out_white_scale), s.out_white);

    char buf[16];
    snprintf(buf, sizeof buf, "%d", s.in_black);  gtk_label_set_text(GTK_LABEL(st->in_black_lbl), buf);
    snprintf(buf, sizeof buf, "%d", s.in_white);  gtk_label_set_text(GTK_LABEL(st->in_white_lbl), buf);
    snprintf(buf, sizeof buf, "%.2f", s.gamma);   gtk_label_set_text(GTK_LABEL(st->gamma_lbl), buf);
    snprintf(buf, sizeof buf, "%d", s.out_black); gtk_label_set_text(GTK_LABEL(st->out_black_lbl), buf);
    snprintf(buf, sizeof buf, "%d", s.out_white); gtk_label_set_text(GTK_LABEL(st->out_white_lbl), buf);

    regenerate(st);
    gtk_widget_queue_draw(st->hist_area);
}

static void push_snapshot(LevelsState *st) {
    LevelsSnapshot *s = g_new0(LevelsSnapshot, 1);
    *s = levels_snapshot(st);
    g_ptr_array_add(st->undo_stack, s);
    gtk_widget_set_sensitive(st->undo_btn, TRUE);
}

static void on_in_black(GtkRange *r, gpointer d) {"""
code = code.replace('static void on_in_black(GtkRange *r, gpointer d) {', helpers)

# 3. Modify callbacks
code = code.replace("""static void on_in_black(GtkRange *r, gpointer d) {
    LevelsState *st = get_state(d);
    st->in_black = (int)gtk_range_get_value(r);""", """static void on_in_black(GtkRange *r, gpointer d) {
    LevelsState *st = get_state(d);
    int new_val = (int)gtk_range_get_value(r);
    if (new_val == st->in_black) return;
    push_snapshot(st);
    st->in_black = new_val;""")

code = code.replace("""static void on_in_white(GtkRange *r, gpointer d) {
    LevelsState *st = get_state(d);
    st->in_white = (int)gtk_range_get_value(r);""", """static void on_in_white(GtkRange *r, gpointer d) {
    LevelsState *st = get_state(d);
    int new_val = (int)gtk_range_get_value(r);
    if (new_val == st->in_white) return;
    push_snapshot(st);
    st->in_white = new_val;""")

code = code.replace("""static void on_gamma(GtkRange *r, gpointer d) {
    LevelsState *st = get_state(d);
    st->gamma = gtk_range_get_value(r);""", """static void on_gamma(GtkRange *r, gpointer d) {
    LevelsState *st = get_state(d);
    double new_val = gtk_range_get_value(r);
    if (fabs(new_val - st->gamma) < 0.001) return;
    push_snapshot(st);
    st->gamma = new_val;""")

code = code.replace("""static void on_out_black(GtkRange *r, gpointer d) {
    LevelsState *st = get_state(d);
    st->out_black = (int)gtk_range_get_value(r);""", """static void on_out_black(GtkRange *r, gpointer d) {
    LevelsState *st = get_state(d);
    int new_val = (int)gtk_range_get_value(r);
    if (new_val == st->out_black) return;
    push_snapshot(st);
    st->out_black = new_val;""")

code = code.replace("""static void on_out_white(GtkRange *r, gpointer d) {
    LevelsState *st = get_state(d);
    st->out_white = (int)gtk_range_get_value(r);""", """static void on_out_white(GtkRange *r, gpointer d) {
    LevelsState *st = get_state(d);
    int new_val = (int)gtk_range_get_value(r);
    if (new_val == st->out_white) return;
    push_snapshot(st);
    st->out_white = new_val;""")

# 4. on_reset -> on_undo + on_reset
reset_old = """static void on_reset(GtkButton *b, gpointer d) {
    (void)b;
    reset_all(get_state(d));
}"""
reset_new = """static void on_undo(GtkButton *b, gpointer d) {
    (void)b;
    LevelsState *st = get_state(d);
    if (st->undo_stack->len == 0) return;

    LevelsSnapshot *s = g_ptr_array_index(st->undo_stack,
                                            st->undo_stack->len - 1);
    g_ptr_array_remove_index(st->undo_stack, st->undo_stack->len - 1);

    levels_restore(st, *s);
    g_free(s);

    gtk_widget_set_sensitive(st->undo_btn, st->undo_stack->len > 0);
}

static void on_reset(GtkButton *b, gpointer d) {
    (void)b;
    LevelsState *st = get_state(d);
    if (!st->original) return;
    push_snapshot(st);
    reset_all(st);
}"""
code = code.replace(reset_old, reset_new)

# 5. on_drop
drop_old = """    st->original = pb;
    st->path = g_strdup(path);"""
drop_new = """    g_clear_object(&st->first_original);
    st->first_original = g_object_ref(pb);
    if (st->undo_stack) {
        for (guint i = 0; i < st->undo_stack->len; i++)
            g_free(g_ptr_array_index(st->undo_stack, i));
        g_ptr_array_set_size(st->undo_stack, 0);
    }
    if (st->undo_btn) gtk_widget_set_sensitive(st->undo_btn, FALSE);

    st->original = pb;
    st->path = g_strdup(path);"""
code = code.replace(drop_old, drop_new)

# 6. lifecycle
lifecycle_old = """void image_levels_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "levels-state", NULL);
}"""
lifecycle_new = """static void levels_state_free(LevelsState *st) {
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

void image_levels_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "levels-state", NULL);
}"""
code = code.replace(lifecycle_old, lifecycle_new)
code = code.replace('g_object_set_data_full(G_OBJECT(root), "levels-state", st, g_free);', 'g_object_set_data_full(G_OBJECT(root), "levels-state", st, (GDestroyNotify)levels_state_free);')

# 7. create
code = code.replace('    LevelsState *st = g_new0(LevelsState, 1);\n    st->in_white = 255;', '    LevelsState *st = g_new0(LevelsState, 1);\n    st->undo_stack = g_ptr_array_new();\n    st->in_white = 255;')

btn_old = """    GtkWidget *reset = gtk_button_new_with_label("Reset");
    gtk_widget_add_css_class(reset, "flat");

    GtkWidget *save = gtk_button_new_with_label("Save As…");
    gtk_widget_add_css_class(save, "flat");

    gtk_box_append(GTK_BOX(btn_row), btn_sp);
    gtk_box_append(GTK_BOX(btn_row), image_new_image_button(on_drop, root));
    gtk_box_append(GTK_BOX(btn_row), reset);
    gtk_box_append(GTK_BOX(btn_row), save);"""

btn_new = """    st->undo_btn = image_undo_button(G_CALLBACK(on_undo), root);
    gtk_widget_set_sensitive(st->undo_btn, FALSE);

    GtkWidget *reset_btn = image_reset_button(G_CALLBACK(on_reset), root);

    GtkWidget *save = gtk_button_new_with_label("Save As…");
    gtk_widget_add_css_class(save, "flat");

    gtk_box_append(GTK_BOX(btn_row), btn_sp);
    gtk_box_append(GTK_BOX(btn_row), image_new_image_button(on_drop, root));
    gtk_box_append(GTK_BOX(btn_row), st->undo_btn);
    gtk_box_append(GTK_BOX(btn_row), reset_btn);
    gtk_box_append(GTK_BOX(btn_row), save);"""
code = code.replace(btn_old, btn_new)
code = code.replace('g_signal_connect(reset, "clicked", G_CALLBACK(on_reset), root);', '')

with open('src/modules/image/tools/image_levels.c', 'w') as f:
    f.write(code)

