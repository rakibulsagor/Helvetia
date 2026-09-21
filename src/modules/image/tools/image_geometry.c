#include <gtk/gtk.h>
#include <adwaita.h>
#include "../image_shared.h"
#include "image_geometry.h"


/* ------------------------------------------------------------------ */
/* Common editor state                                                */
/* ------------------------------------------------------------------ */

typedef struct {
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

static gpointer get_state(GtkWidget *v);
static void geo_update_preview(GeoState *st);

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
}

static gpointer get_state(GtkWidget *v) {
    return g_object_get_data(G_OBJECT(v), "geo-state");
}

static void geo_update_preview(GeoState *st) {
    GdkPixbuf *src = st->preview ? st->preview : st->original;
    if (!src) return;
    GdkTexture *t = gdk_texture_new_for_pixbuf(src);
    gtk_picture_set_paintable(GTK_PICTURE(st->picture), GDK_PAINTABLE(t));
    gtk_picture_set_content_fit(GTK_PICTURE(st->picture),
                                 GTK_CONTENT_FIT_CONTAIN);
    gtk_picture_set_can_shrink(GTK_PICTURE(st->picture), TRUE);
    g_object_unref(t);
}

static void geo_load(GeoState *st, const char *path) {
    GError *e = NULL;
    GdkPixbuf *pb = gdk_pixbuf_new_from_file(path, &e);
    if (!pb) { image_show_error(st->root, e->message); g_error_free(e); return; }

    g_clear_object(&st->original);
    g_clear_object(&st->preview);
    g_clear_object(&st->first_original);
    if (st->undo_stack) image_undo_clear(st->undo_stack);
    geo_update_undo_btn(st);
    g_free(st->path);

    st->original = pb;
    st->first_original = g_object_ref(pb);
    st->path = g_strdup(path);
    st->img_w = gdk_pixbuf_get_width(pb);
    st->img_h = gdk_pixbuf_get_height(pb);

    geo_update_preview(st);
    gtk_stack_set_visible_child_name(GTK_STACK(st->stack), "editor");
}

static void geo_save(GeoState *st, const char *prefix) {
    GdkPixbuf *src = st->preview ? st->preview : st->original;
    if (!src) return;
    char *base = st->path ? g_path_get_basename(st->path) : g_strdup("image.png");
    char *name = g_strdup_printf("%s_%s", prefix, base);
    image_save_pixbuf_dialog(st->root, src, name);
    g_free(name);
    g_free(base);
}

/* ------------------------------------------------------------------ */
/* RESIZE                                                             */
/* ------------------------------------------------------------------ */

typedef struct {
    GeoState base;
    GtkWidget *w_entry, *h_entry, *ratio_switch;
    gboolean   updating;
    double     aspect;
} ResizeState;

static const int PRESET_SIZES[][2] = {
    {0, 0},           /* Custom */
    {1920, 1080},     /* Full HD */
    {1280, 720},      /* HD */
    {800, 600},       /* SVGA */
    {640, 480},       /* VGA */
    {3840, 2160},     /* 4K */
    {2560, 1440},     /* QHD */
};
static const char *PRESET_LABELS[] = {
    "Custom", "1920×1080 (FHD)", "1280×720 (HD)", "800×600",
    "640×480 (VGA)", "3840×2160 (4K)", "2560×1440 (QHD)", NULL
};

static void on_preset_changed(GObject *dd, GParamSpec *p, gpointer d) {
    (void)p;
    ResizeState *st = get_state(d);
    guint i = gtk_drop_down_get_selected(GTK_DROP_DOWN(dd));
    if (i == 0 || i >= G_N_ELEMENTS(PRESET_SIZES)) return;

    st->updating = TRUE;
    char buf[16];
    snprintf(buf, sizeof buf, "%d", PRESET_SIZES[i][0]);
    gtk_editable_set_text(GTK_EDITABLE(st->w_entry), buf);
    snprintf(buf, sizeof buf, "%d", PRESET_SIZES[i][1]);
    gtk_editable_set_text(GTK_EDITABLE(st->h_entry), buf);
    st->updating = FALSE;
}

static void on_resize_w(GtkEditable *e, gpointer d) {
    ResizeState *st = get_state(d);
    if (st->updating) return;
    if (!gtk_switch_get_active(GTK_SWITCH(st->ratio_switch))) return;

    int w = atoi(gtk_editable_get_text(e));
    if (w > 0) {
        int h = (int)(w / st->aspect);
        st->updating = TRUE;
        char buf[32]; snprintf(buf, sizeof buf, "%d", h);
        gtk_editable_set_text(GTK_EDITABLE(st->h_entry), buf);
        st->updating = FALSE;
    }
}

static void on_resize_h(GtkEditable *e, gpointer d) {
    ResizeState *st = get_state(d);
    if (st->updating) return;
    if (!gtk_switch_get_active(GTK_SWITCH(st->ratio_switch))) return;

    int h = atoi(gtk_editable_get_text(e));
    if (h > 0) {
        int w = (int)(h * st->aspect);
        st->updating = TRUE;
        char buf[32]; snprintf(buf, sizeof buf, "%d", w);
        gtk_editable_set_text(GTK_EDITABLE(st->w_entry), buf);
        st->updating = FALSE;
    }
}

static void resize_apply(GtkButton *b, gpointer d) {
    (void)b;
    ResizeState *st = get_state(d);
    if (!st->base.original) return;

    int w = atoi(gtk_editable_get_text(GTK_EDITABLE(st->w_entry)));
    int h = atoi(gtk_editable_get_text(GTK_EDITABLE(st->h_entry)));
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (w > 20000) w = 20000;
    if (h > 20000) h = 20000;

    GdkPixbuf *new = gdk_pixbuf_scale_simple(st->base.original, w, h,
                                              GDK_INTERP_BILINEAR);
    geo_push_undo(&st->base);
    g_clear_object(&st->base.original);
    st->base.original = new;
    st->base.img_w = w;
    st->base.img_h = h;
    g_clear_object(&st->base.preview);
    geo_update_preview(&st->base);
}

static void resize_save(GtkButton *b, gpointer d) {
    (void)b;
    geo_save(get_state(d), "resized_");
}

void image_resize_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "geo-state", NULL);
}

static void resize_on_drop(const char *p, gpointer d) {
    ResizeState *s = get_state(d);
    geo_load(&s->base, p);
    s->aspect = (double)s->base.img_w / s->base.img_h;
    char buf[32];
    snprintf(buf, sizeof buf, "%d", s->base.img_w);
    gtk_editable_set_text(GTK_EDITABLE(s->w_entry), buf);
    snprintf(buf, sizeof buf, "%d", s->base.img_h);
    gtk_editable_set_text(GTK_EDITABLE(s->h_entry), buf);
}

static void resize_pct_clicked(GtkButton *b, gpointer d) {
    ResizeState *s = get_state(d);
    if (!s->base.original) return;
    int pct = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "pct"));
    int w = s->base.img_w * pct / 100;
    int h = s->base.img_h * pct / 100;
    s->updating = TRUE;
    char buf[16];
    snprintf(buf, sizeof buf, "%d", w);
    gtk_editable_set_text(GTK_EDITABLE(s->w_entry), buf);
    snprintf(buf, sizeof buf, "%d", h);
    gtk_editable_set_text(GTK_EDITABLE(s->h_entry), buf);
    s->updating = FALSE;
    resize_apply(NULL, d);
}

GtkWidget *image_resize_create(void) {
    ResizeState *st = g_new0(ResizeState, 1);
    st->base.undo_stack = image_undo_stack_new();
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->base.root = root;
    image_install_edit_shortcuts(root,
        (ImageToolCallback)geo_undo,
        (ImageToolCallback)geo_reset,
        root);

    GtkWidget *stack = gtk_stack_new();
    gtk_widget_set_vexpand(stack, TRUE);
    st->base.stack = stack;

    GtkWidget *drop_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(drop_box, 24);
    gtk_widget_set_margin_end(drop_box, 24);
    gtk_widget_set_margin_top(drop_box, 24);
    gtk_widget_set_margin_bottom(drop_box, 24);
    gtk_widget_set_vexpand(drop_box, TRUE);
    gtk_box_append(GTK_BOX(drop_box),
        image_build_drop_zone("Image file", resize_on_drop, root));
    gtk_stack_add_named(GTK_STACK(stack), drop_box, "drop");

    GtkWidget *editor = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(bar, 12);
    gtk_widget_set_margin_end(bar, 12);
    gtk_widget_set_margin_top(bar, 8);
    gtk_widget_set_margin_bottom(bar, 8);

    GtkWidget *preset_dd = gtk_drop_down_new_from_strings(PRESET_LABELS);
    g_signal_connect(preset_dd, "notify::selected",
                     G_CALLBACK(on_preset_changed), root);
    gtk_box_append(GTK_BOX(bar), preset_dd);

    /* Percentage quick buttons */
    static const int PCTS[] = { 25, 50, 75 };
    const char *pct_labels[] = { "25%", "50%", "75%", NULL };
    for (int i = 0; i < 3; i++) {
        GtkWidget *btn = gtk_button_new_with_label(pct_labels[i]);
        gtk_widget_add_css_class(btn, "flat");
        g_object_set_data(G_OBJECT(btn), "pct", GINT_TO_POINTER(PCTS[i]));
        g_signal_connect(btn, "clicked", G_CALLBACK(resize_pct_clicked), root);
        gtk_box_append(GTK_BOX(bar), btn);
    }

    gtk_box_append(GTK_BOX(bar), gtk_separator_new(GTK_ORIENTATION_VERTICAL));

    gtk_box_append(GTK_BOX(bar), gtk_label_new("Width:"));
    st->w_entry = gtk_entry_new();
    gtk_widget_set_size_request(st->w_entry, 80, -1);
    gtk_box_append(GTK_BOX(bar), st->w_entry);

    gtk_box_append(GTK_BOX(bar), gtk_label_new("Height:"));
    st->h_entry = gtk_entry_new();
    gtk_widget_set_size_request(st->h_entry, 80, -1);
    gtk_box_append(GTK_BOX(bar), st->h_entry);

    gtk_box_append(GTK_BOX(bar), gtk_label_new("Keep ratio"));
    st->ratio_switch = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(st->ratio_switch), TRUE);
    gtk_box_append(GTK_BOX(bar), st->ratio_switch);

    st->base.undo_btn = image_undo_button(G_CALLBACK(geo_undo), root);
    GtkWidget *reset_btn = image_reset_button(G_CALLBACK(geo_reset), root);
    gtk_box_append(GTK_BOX(bar), image_new_image_button(resize_on_drop, root));
    gtk_box_append(GTK_BOX(bar), st->base.undo_btn);
    gtk_box_append(GTK_BOX(bar), reset_btn);


    GtkWidget *sp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(sp, TRUE);
    gtk_box_append(GTK_BOX(bar), sp);

    GtkWidget *apply = gtk_button_new_with_label("Apply");
    gtk_widget_add_css_class(apply, "suggested-action");
    gtk_box_append(GTK_BOX(bar), apply);

    GtkWidget *save = gtk_button_new_with_label("Save As…");
    gtk_widget_add_css_class(save, "flat");
    gtk_box_append(GTK_BOX(bar), save);

    GtkWidget *pic = gtk_picture_new();
    gtk_widget_set_vexpand(pic, TRUE);
    st->base.picture = pic;

    gtk_box_append(GTK_BOX(editor), bar);
    gtk_box_append(GTK_BOX(editor),
                   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(editor), pic);
    gtk_stack_add_named(GTK_STACK(stack), editor, "editor");

    gtk_stack_set_visible_child_name(GTK_STACK(stack), "drop");
    gtk_box_append(GTK_BOX(root), stack);

    g_object_set_data_full(G_OBJECT(root), "geo-state", st, geo_state_free);
    g_signal_connect(st->w_entry, "changed", G_CALLBACK(on_resize_w), root);
    g_signal_connect(st->h_entry, "changed", G_CALLBACK(on_resize_h), root);
    g_signal_connect(apply, "clicked", G_CALLBACK(resize_apply), root);
    g_signal_connect(save, "clicked", G_CALLBACK(resize_save), root);

    return root;
}

/* ------------------------------------------------------------------ */
/* ROTATE                                                             */
/* ------------------------------------------------------------------ */

typedef struct {
    GeoState base;
    int        total_rotation;   /* accumulated: 0, 90, 180, 270 */
} RotateState;

static RotateState *get_rot_state(GtkWidget *v) {
    return g_object_get_data(G_OBJECT(v), "geo-state");
}

static void rotate_apply(RotateState *st, GdkPixbufRotation rot) {
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
    (void)b;
    rotate_apply(get_rot_state(d), GDK_PIXBUF_ROTATE_CLOCKWISE);
}
static void do_rotate_ccw(GtkButton *b, gpointer d) {
    (void)b;
    rotate_apply(get_rot_state(d), GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE);
}
static void do_rotate_180(GtkButton *b, gpointer d) {
    (void)b;
    rotate_apply(get_rot_state(d), GDK_PIXBUF_ROTATE_UPSIDEDOWN);
}

static void on_rot_save(GtkButton *b, gpointer d) {
    (void)b;
    geo_save(get_state(d), "rotated_");
}

void image_rotate_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "geo-state", NULL);
}

static void rotate_on_drop(const char *p, gpointer d) {
    RotateState *s = get_rot_state(d);
    s->total_rotation = 0;
    geo_load(&s->base, p);
}

GtkWidget *image_rotate_create(void) {
    RotateState *st = g_new0(RotateState, 1);
    st->base.undo_stack = image_undo_stack_new();
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->base.root = root;
    image_install_edit_shortcuts(root,
        (ImageToolCallback)geo_undo,
        (ImageToolCallback)geo_reset,
        root);

    GtkWidget *stack = gtk_stack_new();
    gtk_widget_set_vexpand(stack, TRUE);
    st->base.stack = stack;

    GtkWidget *drop_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(drop_box, 24);
    gtk_widget_set_margin_end(drop_box, 24);
    gtk_widget_set_margin_top(drop_box, 24);
    gtk_widget_set_margin_bottom(drop_box, 24);
    gtk_widget_set_vexpand(drop_box, TRUE);
    gtk_box_append(GTK_BOX(drop_box),
        image_build_drop_zone("Image file", rotate_on_drop, root));
    gtk_stack_add_named(GTK_STACK(stack), drop_box, "drop");

    GtkWidget *editor = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_margin_start(bar, 12);
    gtk_widget_set_margin_end(bar, 12);
    gtk_widget_set_margin_top(bar, 8);
    gtk_widget_set_margin_bottom(bar, 8);

    /* Immediate-action buttons */
    GtkWidget *ccw = gtk_button_new_from_icon_name("object-rotate-left-symbolic");
    GtkWidget *cw  = gtk_button_new_from_icon_name("object-rotate-right-symbolic");
    GtkWidget *r180 = gtk_button_new_with_label("180°");
    gtk_widget_set_tooltip_text(ccw, "Rotate 90° counter-clockwise");
    gtk_widget_set_tooltip_text(cw, "Rotate 90° clockwise");
    gtk_widget_set_tooltip_text(r180, "Rotate 180°");

    gtk_box_append(GTK_BOX(bar), ccw);
    gtk_box_append(GTK_BOX(bar), cw);
    gtk_box_append(GTK_BOX(bar), r180);

    st->base.undo_btn = image_undo_button(G_CALLBACK(geo_undo), root);
    GtkWidget *reset_btn = image_reset_button(G_CALLBACK(geo_reset), root);
    gtk_box_append(GTK_BOX(bar), image_new_image_button(rotate_on_drop, root));
    gtk_box_append(GTK_BOX(bar), st->base.undo_btn);
    gtk_box_append(GTK_BOX(bar), reset_btn);


    GtkWidget *sp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(sp, TRUE);
    gtk_box_append(GTK_BOX(bar), sp);

    GtkWidget *save = gtk_button_new_with_label("Save As…");
    gtk_widget_add_css_class(save, "flat");
    gtk_box_append(GTK_BOX(bar), save);

    GtkWidget *pic = gtk_picture_new();
    gtk_widget_set_vexpand(pic, TRUE);
    gtk_widget_set_halign(pic, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(pic, GTK_ALIGN_CENTER);
    st->base.picture = pic;

    gtk_box_append(GTK_BOX(editor), bar);
    gtk_box_append(GTK_BOX(editor),
                   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(editor), pic);
    gtk_stack_add_named(GTK_STACK(stack), editor, "editor");

    gtk_stack_set_visible_child_name(GTK_STACK(stack), "drop");
    gtk_box_append(GTK_BOX(root), stack);

    g_object_set_data_full(G_OBJECT(root), "geo-state", st, geo_state_free);

    g_signal_connect(ccw,  "clicked", G_CALLBACK(do_rotate_ccw), root);
    g_signal_connect(cw,   "clicked", G_CALLBACK(do_rotate_cw),  root);
    g_signal_connect(r180, "clicked", G_CALLBACK(do_rotate_180), root);
    g_signal_connect(save, "clicked", G_CALLBACK(on_rot_save),   root);

    return root;
}

/* ------------------------------------------------------------------ */
/* FLIP                                                               */
/* ------------------------------------------------------------------ */

typedef struct {
    GeoState base;
    gboolean horizontal;
} FlipState;

static void flip_apply(GtkButton *b, gpointer d) {
    (void)b;
    FlipState *st = get_state(d);
    if (!st->base.original) return;

    GdkPixbuf *new = gdk_pixbuf_flip(st->base.original, st->horizontal);
    geo_push_undo(&st->base);
    g_clear_object(&st->base.original);
    st->base.original = new;
    st->base.img_w = gdk_pixbuf_get_width(new);
    st->base.img_h = gdk_pixbuf_get_height(new);
    g_clear_object(&st->base.preview);
    geo_update_preview(&st->base);
}

static void flip_save(GtkButton *b, gpointer d) {
    (void)b;
    geo_save(get_state(d), "flipped_");
}

static void on_flip_dir(GObject *dd, GParamSpec *p, gpointer d) {
    (void)p;
    FlipState *st = get_state(d);
    st->horizontal = gtk_drop_down_get_selected(GTK_DROP_DOWN(dd)) == 0;
}

void image_flip_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "geo-state", NULL);
}

static void flip_on_drop(const char *p, gpointer d) {
    FlipState *st = get_state(d);
    geo_load(&st->base, p);
}

GtkWidget *image_flip_create(void) {
    FlipState *st = g_new0(FlipState, 1);
    st->base.undo_stack = image_undo_stack_new();
    st->horizontal = TRUE;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->base.root = root;
    image_install_edit_shortcuts(root,
        (ImageToolCallback)geo_undo,
        (ImageToolCallback)geo_reset,
        root);

    GtkWidget *stack = gtk_stack_new();
    gtk_widget_set_vexpand(stack, TRUE);
    st->base.stack = stack;

    GtkWidget *drop_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(drop_box, 24);
    gtk_widget_set_margin_end(drop_box, 24);
    gtk_widget_set_margin_top(drop_box, 24);
    gtk_widget_set_margin_bottom(drop_box, 24);
    gtk_widget_set_vexpand(drop_box, TRUE);
    gtk_box_append(GTK_BOX(drop_box),
        image_build_drop_zone("Image file", flip_on_drop, root));
    gtk_stack_add_named(GTK_STACK(stack), drop_box, "drop");

    GtkWidget *editor = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(bar, 12);
    gtk_widget_set_margin_end(bar, 12);
    gtk_widget_set_margin_top(bar, 8);
    gtk_widget_set_margin_bottom(bar, 8);

    gtk_box_append(GTK_BOX(bar), gtk_label_new("Direction:"));
    const char *dirs[] = {"Horizontal", "Vertical", NULL};
    GtkWidget *dd = gtk_drop_down_new_from_strings(dirs);
    gtk_box_append(GTK_BOX(bar), dd);

    st->base.undo_btn = image_undo_button(G_CALLBACK(geo_undo), root);
    GtkWidget *reset_btn = image_reset_button(G_CALLBACK(geo_reset), root);
    gtk_box_append(GTK_BOX(bar), image_new_image_button(flip_on_drop, root));
    gtk_box_append(GTK_BOX(bar), st->base.undo_btn);
    gtk_box_append(GTK_BOX(bar), reset_btn);


    GtkWidget *sp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(sp, TRUE);
    gtk_box_append(GTK_BOX(bar), sp);

    GtkWidget *apply = gtk_button_new_with_label("Apply");
    gtk_widget_add_css_class(apply, "suggested-action");
    gtk_box_append(GTK_BOX(bar), apply);
    GtkWidget *save = gtk_button_new_with_label("Save As…");
    gtk_widget_add_css_class(save, "flat");
    gtk_box_append(GTK_BOX(bar), save);

    GtkWidget *pic = gtk_picture_new();
    gtk_widget_set_vexpand(pic, TRUE);
    st->base.picture = pic;

    gtk_box_append(GTK_BOX(editor), bar);
    gtk_box_append(GTK_BOX(editor),
                   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(editor), pic);
    gtk_stack_add_named(GTK_STACK(stack), editor, "editor");

    gtk_stack_set_visible_child_name(GTK_STACK(stack), "drop");
    gtk_box_append(GTK_BOX(root), stack);

    g_object_set_data_full(G_OBJECT(root), "geo-state", st, geo_state_free);
    g_signal_connect(dd, "notify::selected", G_CALLBACK(on_flip_dir), root);
    g_signal_connect(apply, "clicked", G_CALLBACK(flip_apply), root);
    g_signal_connect(save, "clicked", G_CALLBACK(flip_save), root);

    return root;
}

/* ------------------------------------------------------------------ */
/* CANVAS RESIZE                                                      */
/* ------------------------------------------------------------------ */

typedef struct {
    GeoState base;
    GtkWidget *w_entry, *h_entry;
} CanvasState;

static void canvas_apply(GtkButton *b, gpointer d) {
    (void)b;
    CanvasState *st = get_state(d);
    if (!st->base.original) return;

    int w = atoi(gtk_editable_get_text(GTK_EDITABLE(st->w_entry)));
    int h = atoi(gtk_editable_get_text(GTK_EDITABLE(st->h_entry)));
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (w > 20000) w = 20000;
    if (h > 20000) h = 20000;

    GdkPixbuf *new = gdk_pixbuf_new(
        gdk_pixbuf_get_colorspace(st->base.original),
        gdk_pixbuf_get_has_alpha(st->base.original),
        gdk_pixbuf_get_bits_per_sample(st->base.original),
        w, h);

    gdk_pixbuf_fill(new, 0x00000000);

    /* Center the original in the new canvas */
    int ox = (w - st->base.img_w) / 2;
    int oy = (h - st->base.img_h) / 2;
    int cw = MIN(st->base.img_w, w);
    int ch = MIN(st->base.img_h, h);
    int sx = MAX(0, -ox);
    int sy = MAX(0, -oy);

    gdk_pixbuf_copy_area(st->base.original, sx, sy, cw, ch,
                         new, MAX(0, ox), MAX(0, oy));

    geo_push_undo(&st->base);
    g_clear_object(&st->base.original);
    st->base.original = new;
    st->base.img_w = w;
    st->base.img_h = h;
    g_clear_object(&st->base.preview);
    geo_update_preview(&st->base);
}

static void canvas_save(GtkButton *b, gpointer d) {
    (void)b;
    geo_save(get_state(d), "canvas_");
}

void image_canvas_resize_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "geo-state", NULL);
}

static void canvas_resize_on_drop(const char *p, gpointer d) {
    CanvasState *s = get_state(d);
    geo_load(&s->base, p);
    char buf[32];
    snprintf(buf, sizeof buf, "%d", s->base.img_w);
    gtk_editable_set_text(GTK_EDITABLE(s->w_entry), buf);
    snprintf(buf, sizeof buf, "%d", s->base.img_h);
    gtk_editable_set_text(GTK_EDITABLE(s->h_entry), buf);
}

GtkWidget *image_canvas_resize_create(void) {
    CanvasState *st = g_new0(CanvasState, 1);
    st->base.undo_stack = image_undo_stack_new();
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->base.root = root;
    image_install_edit_shortcuts(root,
        (ImageToolCallback)geo_undo,
        (ImageToolCallback)geo_reset,
        root);

    GtkWidget *stack = gtk_stack_new();
    gtk_widget_set_vexpand(stack, TRUE);
    st->base.stack = stack;

    GtkWidget *drop_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(drop_box, 24);
    gtk_widget_set_margin_end(drop_box, 24);
    gtk_widget_set_margin_top(drop_box, 24);
    gtk_widget_set_margin_bottom(drop_box, 24);
    gtk_widget_set_vexpand(drop_box, TRUE);
    gtk_box_append(GTK_BOX(drop_box),
        image_build_drop_zone("Image file", canvas_resize_on_drop, root));
    gtk_stack_add_named(GTK_STACK(stack), drop_box, "drop");

    GtkWidget *editor = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(bar, 12);
    gtk_widget_set_margin_end(bar, 12);
    gtk_widget_set_margin_top(bar, 8);
    gtk_widget_set_margin_bottom(bar, 8);

    gtk_box_append(GTK_BOX(bar), gtk_label_new("Canvas W:"));
    st->w_entry = gtk_entry_new();
    gtk_widget_set_size_request(st->w_entry, 80, -1);
    gtk_box_append(GTK_BOX(bar), st->w_entry);

    gtk_box_append(GTK_BOX(bar), gtk_label_new("H:"));
    st->h_entry = gtk_entry_new();
    gtk_widget_set_size_request(st->h_entry, 80, -1);
    gtk_box_append(GTK_BOX(bar), st->h_entry);

    st->base.undo_btn = image_undo_button(G_CALLBACK(geo_undo), root);
    GtkWidget *reset_btn = image_reset_button(G_CALLBACK(geo_reset), root);
    gtk_box_append(GTK_BOX(bar), image_new_image_button(canvas_resize_on_drop, root));
    gtk_box_append(GTK_BOX(bar), st->base.undo_btn);
    gtk_box_append(GTK_BOX(bar), reset_btn);


    GtkWidget *sp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(sp, TRUE);
    gtk_box_append(GTK_BOX(bar), sp);

    GtkWidget *apply = gtk_button_new_with_label("Apply");
    gtk_widget_add_css_class(apply, "suggested-action");
    gtk_box_append(GTK_BOX(bar), apply);
    GtkWidget *save = gtk_button_new_with_label("Save As…");
    gtk_widget_add_css_class(save, "flat");
    gtk_box_append(GTK_BOX(bar), save);

    GtkWidget *pic = gtk_picture_new();
    gtk_widget_set_vexpand(pic, TRUE);
    st->base.picture = pic;

    gtk_box_append(GTK_BOX(editor), bar);
    gtk_box_append(GTK_BOX(editor),
                   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(editor), pic);
    gtk_stack_add_named(GTK_STACK(stack), editor, "editor");

    gtk_stack_set_visible_child_name(GTK_STACK(stack), "drop");
    gtk_box_append(GTK_BOX(root), stack);

    g_object_set_data_full(G_OBJECT(root), "geo-state", st, geo_state_free);
    g_signal_connect(apply, "clicked", G_CALLBACK(canvas_apply), root);
    g_signal_connect(save, "clicked", G_CALLBACK(canvas_save), root);

    return root;
}
