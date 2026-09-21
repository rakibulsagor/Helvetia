import re

with open('src/modules/image/tools/image_geometry.c', 'r') as f:
    code = f.read()

# 1. GeoState
geo_state_old = """typedef struct {
    GdkPixbuf *original;
    GdkPixbuf *preview;
    char      *path;
    int        img_w, img_h;

    GtkWidget *stack, *picture, *root;
    GtkWidget *apply_btn;
} GeoState;"""

geo_state_new = """typedef struct {
    GdkPixbuf *original;
    GdkPixbuf *first_original;
    GPtrArray *undo_stack;
    GdkPixbuf *preview;
    char      *path;
    int        img_w, img_h;

    GtkWidget *stack, *picture, *root;
    GtkWidget *apply_btn;
    GtkWidget *undo_btn;
} GeoState;

static void geo_update_undo_btn(GeoState *st) {
    if (st->undo_btn)
        gtk_widget_set_sensitive(st->undo_btn, st->undo_stack && st->undo_stack->len > 0);
}

static void geo_push_undo(GeoState *st) {
    if (st->original) image_undo_push(st->undo_stack, st->original);
    geo_update_undo_btn(st);
}

static void geo_undo(GtkButton *b, gpointer d) {
    (void)b;
    GeoState *st = get_state(d);
    GdkPixbuf *prev = image_undo_pop(st->undo_stack);
    if (!prev) return;
    g_clear_object(&st->original);
    st->original = prev;
    st->img_w = gdk_pixbuf_get_width(prev);
    st->img_h = gdk_pixbuf_get_height(prev);
    g_clear_object(&st->preview);
    geo_update_preview(st);
    geo_update_undo_btn(st);
}

static void geo_reset(GtkButton *b, gpointer d) {
    (void)b;
    GeoState *st = get_state(d);
    if (!st->first_original) return;
    geo_push_undo(st);
    g_clear_object(&st->original);
    st->original = g_object_ref(st->first_original);
    st->img_w = gdk_pixbuf_get_width(st->original);
    st->img_h = gdk_pixbuf_get_height(st->original);
    g_clear_object(&st->preview);
    geo_update_preview(st);
    geo_update_undo_btn(st);
}

static void geo_state_free(gpointer data) {
    GeoState *st = data;
    if (!st) return;
    g_clear_object(&st->original);
    g_clear_object(&st->preview);
    g_clear_object(&st->first_original);
    if (st->undo_stack) image_undo_free(st->undo_stack);
    g_free(st->path);
    g_free(st);
}"""
code = code.replace(geo_state_old, geo_state_new)

# 2. geo_load
geo_load_old = """    g_clear_object(&st->original);
    g_clear_object(&st->preview);
    g_free(st->path);

    st->original = pb;
    st->path = g_strdup(path);
    st->img_w = gdk_pixbuf_get_width(pb);
    st->img_h = gdk_pixbuf_get_height(pb);"""

geo_load_new = """    g_clear_object(&st->original);
    g_clear_object(&st->preview);
    g_clear_object(&st->first_original);
    if (st->undo_stack) image_undo_clear(st->undo_stack);
    geo_update_undo_btn(st);
    g_free(st->path);

    st->original = pb;
    st->first_original = g_object_ref(pb);
    st->path = g_strdup(path);
    st->img_w = gdk_pixbuf_get_width(pb);
    st->img_h = gdk_pixbuf_get_height(pb);"""
code = code.replace(geo_load_old, geo_load_new)

# 3. Geo frees
code = code.replace('g_object_set_data_full(G_OBJECT(root), "geo-state", st, g_free);', 'g_object_set_data_full(G_OBJECT(root), "geo-state", st, geo_state_free);')

# 4. Resize
resize_apply_old = """    GdkPixbuf *new = gdk_pixbuf_scale_simple(st->base.original, w, h,
                                              GDK_INTERP_BILINEAR);
    g_clear_object(&st->base.preview);
    st->base.preview = new;
    geo_update_preview(&st->base);
    image_show_info(st->base.root, "Preview updated");"""
resize_apply_new = """    GdkPixbuf *new = gdk_pixbuf_scale_simple(st->base.original, w, h,
                                              GDK_INTERP_BILINEAR);
    geo_push_undo(&st->base);
    g_clear_object(&st->base.original);
    st->base.original = new;
    st->base.img_w = w;
    st->base.img_h = h;
    g_clear_object(&st->base.preview);
    geo_update_preview(&st->base);"""
code = code.replace(resize_apply_old, resize_apply_new)

code = code.replace('    ResizeState *st = g_new0(ResizeState, 1);\n    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);', '    ResizeState *st = g_new0(ResizeState, 1);\n    st->base.undo_stack = image_undo_stack_new();\n    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);')

btn_add = """
    st->base.undo_btn = image_undo_button(G_CALLBACK(geo_undo), root);
    GtkWidget *reset_btn = image_reset_button(G_CALLBACK(geo_reset), root);
    gtk_box_append(GTK_BOX(bar), image_new_image_button(resize_on_drop, root));
    gtk_box_append(GTK_BOX(bar), st->base.undo_btn);
    gtk_box_append(GTK_BOX(bar), reset_btn);
"""
code = code.replace('    gtk_box_append(GTK_BOX(bar), image_new_image_button(resize_on_drop, root));', btn_add)


# 5. Rotate
rotate_old = """/* Re-render the preview from the *original* with the current rotation */
static void rotate_refresh(RotateState *st) {
    if (!st->base.original) return;

    GdkPixbuf *new = NULL;
    switch (st->total_rotation) {
        case 90:
            new = gdk_pixbuf_rotate_simple(st->base.original,
                                            GDK_PIXBUF_ROTATE_CLOCKWISE);
            break;
        case 180:
            new = gdk_pixbuf_rotate_simple(st->base.original,
                                            GDK_PIXBUF_ROTATE_UPSIDEDOWN);
            break;
        case 270:
            new = gdk_pixbuf_rotate_simple(st->base.original,
                                            GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE);
            break;
        default:
            new = g_object_ref(st->base.original);
            break;
    }

    g_clear_object(&st->base.preview);
    st->base.preview = new;
    geo_update_preview(&st->base);
}

/* Immediate actions */
static void do_rotate_cw(GtkButton *b, gpointer d) {
    (void)b;
    RotateState *st = get_rot_state(d);
    if (!st->base.original) return;
    st->total_rotation = (st->total_rotation + 90) % 360;
    rotate_refresh(st);
}

static void do_rotate_ccw(GtkButton *b, gpointer d) {
    (void)b;
    RotateState *st = get_rot_state(d);
    if (!st->base.original) return;
    st->total_rotation = (st->total_rotation + 270) % 360;
    rotate_refresh(st);
}

static void do_rotate_180(GtkButton *b, gpointer d) {
    (void)b;
    RotateState *st = get_rot_state(d);
    if (!st->base.original) return;
    st->total_rotation = (st->total_rotation + 180) % 360;
    rotate_refresh(st);
}"""
rotate_new = """static void rotate_apply(RotateState *st, GdkPixbufRotation rot) {
    if (!st->base.original) return;
    geo_push_undo(&st->base);
    GdkPixbuf *new = gdk_pixbuf_rotate_simple(st->base.original, rot);
    g_clear_object(&st->base.original);
    st->base.original = new;
    st->base.img_w = gdk_pixbuf_get_width(new);
    st->base.img_h = gdk_pixbuf_get_height(new);
    g_clear_object(&st->base.preview);
    geo_update_preview(&st->base);
}

static void do_rotate_cw(GtkButton *b, gpointer d) {
    rotate_apply(get_rot_state(d), GDK_PIXBUF_ROTATE_CLOCKWISE);
}
static void do_rotate_ccw(GtkButton *b, gpointer d) {
    rotate_apply(get_rot_state(d), GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE);
}
static void do_rotate_180(GtkButton *b, gpointer d) {
    rotate_apply(get_rot_state(d), GDK_PIXBUF_ROTATE_UPSIDEDOWN);
}"""
code = code.replace(rotate_old, rotate_new)
code = code.replace('    RotateState *st = g_new0(RotateState, 1);\n    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);', '    RotateState *st = g_new0(RotateState, 1);\n    st->base.undo_stack = image_undo_stack_new();\n    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);')

rbtn_add = """
    st->base.undo_btn = image_undo_button(G_CALLBACK(geo_undo), root);
    GtkWidget *reset_btn = image_reset_button(G_CALLBACK(geo_reset), root);
    gtk_box_append(GTK_BOX(bar), image_new_image_button(rotate_on_drop, root));
    gtk_box_append(GTK_BOX(bar), st->base.undo_btn);
    gtk_box_append(GTK_BOX(bar), reset_btn);
"""
code = code.replace('    gtk_box_append(GTK_BOX(bar), image_new_image_button(rotate_on_drop, root));', rbtn_add)

# 6. Flip
flip_apply_old = """    GdkPixbuf *new = gdk_pixbuf_flip(st->base.original, st->horizontal);
    g_clear_object(&st->base.preview);
    st->base.preview = new;
    geo_update_preview(&st->base);"""
flip_apply_new = """    GdkPixbuf *new = gdk_pixbuf_flip(st->base.original, st->horizontal);
    geo_push_undo(&st->base);
    g_clear_object(&st->base.original);
    st->base.original = new;
    st->base.img_w = gdk_pixbuf_get_width(new);
    st->base.img_h = gdk_pixbuf_get_height(new);
    g_clear_object(&st->base.preview);
    geo_update_preview(&st->base);"""
code = code.replace(flip_apply_old, flip_apply_new)

code = code.replace('    FlipState *st = g_new0(FlipState, 1);\n    st->horizontal = TRUE;\n\n    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);', '    FlipState *st = g_new0(FlipState, 1);\n    st->base.undo_stack = image_undo_stack_new();\n    st->horizontal = TRUE;\n\n    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);')

fbtn_add = """
    st->base.undo_btn = image_undo_button(G_CALLBACK(geo_undo), root);
    GtkWidget *reset_btn = image_reset_button(G_CALLBACK(geo_reset), root);
    gtk_box_append(GTK_BOX(bar), image_new_image_button(flip_on_drop, root));
    gtk_box_append(GTK_BOX(bar), st->base.undo_btn);
    gtk_box_append(GTK_BOX(bar), reset_btn);
"""
code = code.replace('    gtk_box_append(GTK_BOX(bar), image_new_image_button(flip_on_drop, root));', fbtn_add)


# 7. Canvas
canvas_apply_old = """    gdk_pixbuf_copy_area(st->base.original, sx, sy, cw, ch,
                         new, MAX(0, ox), MAX(0, oy));

    g_clear_object(&st->base.preview);
    st->base.preview = new;
    geo_update_preview(&st->base);"""

canvas_apply_new = """    gdk_pixbuf_copy_area(st->base.original, sx, sy, cw, ch,
                         new, MAX(0, ox), MAX(0, oy));

    geo_push_undo(&st->base);
    g_clear_object(&st->base.original);
    st->base.original = new;
    st->base.img_w = w;
    st->base.img_h = h;
    g_clear_object(&st->base.preview);
    geo_update_preview(&st->base);"""
code = code.replace(canvas_apply_old, canvas_apply_new)

code = code.replace('    CanvasState *st = g_new0(CanvasState, 1);\n    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);', '    CanvasState *st = g_new0(CanvasState, 1);\n    st->base.undo_stack = image_undo_stack_new();\n    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);')

cbtn_add = """
    st->base.undo_btn = image_undo_button(G_CALLBACK(geo_undo), root);
    GtkWidget *reset_btn = image_reset_button(G_CALLBACK(geo_reset), root);
    gtk_box_append(GTK_BOX(bar), image_new_image_button(canvas_resize_on_drop, root));
    gtk_box_append(GTK_BOX(bar), st->base.undo_btn);
    gtk_box_append(GTK_BOX(bar), reset_btn);
"""
code = code.replace('    gtk_box_append(GTK_BOX(bar), image_new_image_button(canvas_resize_on_drop, root));', cbtn_add)


with open('src/modules/image/tools/image_geometry.c', 'w') as f:
    f.write(code)

