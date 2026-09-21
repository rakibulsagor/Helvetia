import re

with open('src/modules/image/tools/image_autocrop.c', 'r') as f:
    code = f.read()

# 1. AcState
ac_state_old = """typedef struct {
    GdkPixbuf *original;
    GdkPixbuf *preview;
    char      *path;
    int        img_w, img_h;
    GtkWidget *stack, *picture, *root;
} AcState;"""

ac_state_new = """typedef struct {
    GdkPixbuf *original;
    GdkPixbuf *first_original;
    GPtrArray *undo_stack;
    GdkPixbuf *preview;
    char      *path;
    int        img_w, img_h;
    GtkWidget *stack, *picture, *root;
    GtkWidget *undo_btn;
} AcState;

static void ac_update_undo_btn(AcState *st) {
    if (st->undo_btn)
        gtk_widget_set_sensitive(st->undo_btn, st->undo_stack && st->undo_stack->len > 0);
}

static void ac_push_undo(AcState *st) {
    if (st->original) image_undo_push(st->undo_stack, st->original);
    ac_update_undo_btn(st);
}

static void ac_undo(GtkButton *b, gpointer d) {
    (void)b;
    AcState *st = get_state(d);
    GdkPixbuf *prev = image_undo_pop(st->undo_stack);
    if (!prev) return;
    g_clear_object(&st->original);
    st->original = prev;
    st->img_w = gdk_pixbuf_get_width(prev);
    st->img_h = gdk_pixbuf_get_height(prev);
    g_clear_object(&st->preview);
    ac_update(st);
    ac_update_undo_btn(st);
}

static void ac_reset(GtkButton *b, gpointer d) {
    (void)b;
    AcState *st = get_state(d);
    if (!st->first_original) return;
    ac_push_undo(st);
    g_clear_object(&st->original);
    st->original = g_object_ref(st->first_original);
    st->img_w = gdk_pixbuf_get_width(st->original);
    st->img_h = gdk_pixbuf_get_height(st->original);
    g_clear_object(&st->preview);
    ac_update(st);
    ac_update_undo_btn(st);
}

static void ac_state_free(gpointer data) {
    AcState *st = data;
    if (!st) return;
    g_clear_object(&st->original);
    g_clear_object(&st->preview);
    g_clear_object(&st->first_original);
    if (st->undo_stack) image_undo_free(st->undo_stack);
    g_free(st->path);
    g_free(st);
}"""
code = code.replace(ac_state_old, ac_state_new)

# 2. ac_load
ac_load_old = """    g_clear_object(&st->original);
    g_clear_object(&st->preview);
    g_free(st->path);
    st->original = pb;
    st->path = g_strdup(path);
    st->img_w = gdk_pixbuf_get_width(pb);
    st->img_h = gdk_pixbuf_get_height(pb);"""

ac_load_new = """    g_clear_object(&st->original);
    g_clear_object(&st->preview);
    g_clear_object(&st->first_original);
    if (st->undo_stack) image_undo_clear(st->undo_stack);
    ac_update_undo_btn(st);
    g_free(st->path);

    st->original = pb;
    st->first_original = g_object_ref(pb);
    st->path = g_strdup(path);
    st->img_w = gdk_pixbuf_get_width(pb);
    st->img_h = gdk_pixbuf_get_height(pb);"""
code = code.replace(ac_load_old, ac_load_new)

# 3. Straighten apply
straighten_apply_old = """    GdkPixbuf *new = rotate_arbitrary(st->base.original, angle);
    g_clear_object(&st->base.preview);
    st->base.preview = new;
    ac_update(&st->base);"""
straighten_apply_new = """    GdkPixbuf *new = rotate_arbitrary(st->base.original, angle);
    ac_push_undo(&st->base);
    g_clear_object(&st->base.original);
    st->base.original = new;
    st->base.img_w = gdk_pixbuf_get_width(new);
    st->base.img_h = gdk_pixbuf_get_height(new);
    g_clear_object(&st->base.preview);
    ac_update(&st->base);"""
code = code.replace(straighten_apply_old, straighten_apply_new)

code = code.replace('    StraightenState *st = g_new0(StraightenState, 1);\n    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);', '    StraightenState *st = g_new0(StraightenState, 1);\n    st->base.undo_stack = image_undo_stack_new();\n    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);')

code = code.replace('g_object_set_data_full(G_OBJECT(root), "ac-state", st, g_free);', 'g_object_set_data_full(G_OBJECT(root), "ac-state", st, ac_state_free);')

sbtn_add = """
    st->base.undo_btn = image_undo_button(G_CALLBACK(ac_undo), root);
    GtkWidget *reset_btn = image_reset_button(G_CALLBACK(ac_reset), root);
    gtk_box_append(GTK_BOX(bar), image_new_image_button(straighten_on_drop, root));
    gtk_box_append(GTK_BOX(bar), st->base.undo_btn);
    gtk_box_append(GTK_BOX(bar), reset_btn);
"""
code = code.replace('    gtk_box_append(GTK_BOX(bar), image_new_image_button(straighten_on_drop, root));', sbtn_add)

# 4. Autocrop apply
autocrop_apply_old = """    GdkPixbuf *cropped = gdk_pixbuf_new(
        gdk_pixbuf_get_colorspace(src),
        gdk_pixbuf_get_has_alpha(src),
        gdk_pixbuf_get_bits_per_sample(src),
        cw, ch);
    gdk_pixbuf_copy_area(src, left, top, cw, ch, cropped, 0, 0);

    g_clear_object(&st->base.preview);
    st->base.preview = cropped;
    ac_update(&st->base);"""

autocrop_apply_new = """    GdkPixbuf *cropped = gdk_pixbuf_new(
        gdk_pixbuf_get_colorspace(src),
        gdk_pixbuf_get_has_alpha(src),
        gdk_pixbuf_get_bits_per_sample(src),
        cw, ch);
    gdk_pixbuf_copy_area(src, left, top, cw, ch, cropped, 0, 0);

    ac_push_undo(&st->base);
    g_clear_object(&st->base.original);
    st->base.original = cropped;
    st->base.img_w = cw;
    st->base.img_h = ch;
    g_clear_object(&st->base.preview);
    ac_update(&st->base);"""
code = code.replace(autocrop_apply_old, autocrop_apply_new)

code = code.replace('    AutocropState *st = g_new0(AutocropState, 1);\n    st->tolerance = 20;', '    AutocropState *st = g_new0(AutocropState, 1);\n    st->base.undo_stack = image_undo_stack_new();\n    st->tolerance = 20;')

cbtn_add = """
    st->base.undo_btn = image_undo_button(G_CALLBACK(ac_undo), root);
    GtkWidget *reset_btn = image_reset_button(G_CALLBACK(ac_reset), root);
    gtk_box_append(GTK_BOX(bar), image_new_image_button(autocrop_on_drop, root));
    gtk_box_append(GTK_BOX(bar), st->base.undo_btn);
    gtk_box_append(GTK_BOX(bar), reset_btn);
"""
code = code.replace('    gtk_box_append(GTK_BOX(bar), image_new_image_button(autocrop_on_drop, root));', cbtn_add)

with open('src/modules/image/tools/image_autocrop.c', 'w') as f:
    f.write(code)

