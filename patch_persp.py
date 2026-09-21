import re

with open('src/modules/image/tools/image_perspective.c', 'r') as f:
    code = f.read()

# 1. State
state_old = """typedef struct {
    GdkPixbuf *original;
    GdkPixbuf *preview;
    char      *path;
    int        img_w, img_h;
    double     tl_x, tl_y, tr_x, tr_y, bl_x, bl_y, br_x, br_y;
    GtkWidget *stack, *picture, *root;
} PerspState;

static PerspState *get_state(GtkWidget *v) {"""

state_new = """typedef struct {
    GdkPixbuf *original;
    GdkPixbuf *first_original;
    GPtrArray *undo_stack;
    GdkPixbuf *preview;
    char      *path;
    int        img_w, img_h;
    double     tl_x, tl_y, tr_x, tr_y, bl_x, bl_y, br_x, br_y;
    GtkWidget *stack, *picture, *root;
    GtkWidget *undo_btn;
} PerspState;

static PerspState *get_state(GtkWidget *v);
static void update_preview(PerspState *st);

static void persp_update_undo_btn(PerspState *st) {
    if (st->undo_btn)
        gtk_widget_set_sensitive(st->undo_btn, st->undo_stack && st->undo_stack->len > 0);
}

static void persp_push_undo(PerspState *st) {
    if (st->original) image_undo_push(st->undo_stack, st->original);
    persp_update_undo_btn(st);
}

static void persp_undo(GtkButton *b, gpointer d) {
    (void)b;
    PerspState *st = get_state(d);
    GdkPixbuf *prev = image_undo_pop(st->undo_stack);
    if (!prev) return;
    g_clear_object(&st->original);
    st->original = prev;
    st->img_w = gdk_pixbuf_get_width(prev);
    st->img_h = gdk_pixbuf_get_height(prev);
    g_clear_object(&st->preview);
    update_preview(st);
    persp_update_undo_btn(st);
}

static void persp_reset(GtkButton *b, gpointer d) {
    (void)b;
    PerspState *st = get_state(d);
    if (!st->first_original) return;
    persp_push_undo(st);
    g_clear_object(&st->original);
    st->original = g_object_ref(st->first_original);
    st->img_w = gdk_pixbuf_get_width(st->original);
    st->img_h = gdk_pixbuf_get_height(st->original);
    g_clear_object(&st->preview);
    update_preview(st);
    persp_update_undo_btn(st);
}

static void persp_state_free(gpointer data) {
    PerspState *st = data;
    if (!st) return;
    g_clear_object(&st->original);
    g_clear_object(&st->preview);
    g_clear_object(&st->first_original);
    if (st->undo_stack) image_undo_free(st->undo_stack);
    g_free(st->path);
    g_free(st);
}

static PerspState *get_state(GtkWidget *v) {"""
code = code.replace(state_old, state_new)

# 2. apply_persp
apply_old = """static void apply_persp(GtkButton *b, gpointer d) {
    (void)b;
    PerspState *st = get_state(d);
    if (!st->original) return;
    g_clear_object(&st->preview);
    st->preview = apply_perspective(st);
    update_preview(st);
}"""

apply_new = """static void apply_persp(GtkButton *b, gpointer d) {
    (void)b;
    PerspState *st = get_state(d);
    if (!st->original) return;
    
    GdkPixbuf *new = apply_perspective(st);
    persp_push_undo(st);
    g_clear_object(&st->original);
    st->original = new;
    st->img_w = gdk_pixbuf_get_width(new);
    st->img_h = gdk_pixbuf_get_height(new);
    g_clear_object(&st->preview);
    update_preview(st);
}"""
code = code.replace(apply_old, apply_new)

# 3. on_drop
drop_old = """    g_clear_object(&st->original);
    g_clear_object(&st->preview);
    g_free(st->path);
    st->original = pb;
    st->path = g_strdup(path);"""

drop_new = """    g_clear_object(&st->original);
    g_clear_object(&st->preview);
    g_clear_object(&st->first_original);
    if (st->undo_stack) image_undo_clear(st->undo_stack);
    persp_update_undo_btn(st);
    g_free(st->path);

    st->original = pb;
    st->first_original = g_object_ref(pb);
    st->path = g_strdup(path);"""
code = code.replace(drop_old, drop_new)

# 4. lifecycle
code = code.replace('g_object_set_data_full(G_OBJECT(root), "persp-state", st, g_free);', 'g_object_set_data_full(G_OBJECT(root), "persp-state", st, persp_state_free);')

# 5. UI
code = code.replace('    PerspState *st = g_new0(PerspState, 1);', '    PerspState *st = g_new0(PerspState, 1);\n    st->undo_stack = image_undo_stack_new();')

btn_old = """    GtkWidget *btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *apply = gtk_button_new_with_label("Apply");
    gtk_widget_add_css_class(apply, "suggested-action");
    GtkWidget *save = gtk_button_new_with_label("Save As…");
    gtk_widget_add_css_class(save, "flat");
    gtk_box_append(GTK_BOX(btn_row), apply);
    gtk_box_append(GTK_BOX(btn_row), save);
    gtk_box_append(GTK_BOX(opts), btn_row);"""

btn_new = """    GtkWidget *btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    st->undo_btn = image_undo_button(G_CALLBACK(persp_undo), root);
    GtkWidget *reset_btn = image_reset_button(G_CALLBACK(persp_reset), root);
    GtkWidget *apply = gtk_button_new_with_label("Apply");
    gtk_widget_add_css_class(apply, "suggested-action");
    GtkWidget *save = gtk_button_new_with_label("Save As…");
    gtk_widget_add_css_class(save, "flat");
    
    gtk_box_append(GTK_BOX(btn_row), st->undo_btn);
    gtk_box_append(GTK_BOX(btn_row), reset_btn);
    gtk_box_append(GTK_BOX(btn_row), apply);
    gtk_box_append(GTK_BOX(btn_row), save);
    gtk_box_append(GTK_BOX(opts), btn_row);"""
code = code.replace(btn_old, btn_new)

with open('src/modules/image/tools/image_perspective.c', 'w') as f:
    f.write(code)

