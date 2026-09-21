#include <gtk/gtk.h>
#include <adwaita.h>
#include <gio/gio.h>
#include <math.h>

#include "../../../core/tool_registry.h"
#include "../image_shared.h"

/* ------------------------------------------------------------------ */
/* State                                                              */
/* ------------------------------------------------------------------ */

typedef struct {
    /* File */
    char          *path;
    GdkPixbuf     *original;    /* loaded once, kept for re-rendering */
    GdkPixbuf     *rotated;     /* current display pixbuf */
    int            rotation;    /* 0, 90, 180, 270 */

    /* View state */
    double         zoom;        /* 0 = fit mode, >0 = absolute scale */
    gboolean       fit_mode;

    /* Folder navigation */
    GPtrArray     *folder_images;   /* char* — sorted paths */
    int            current_index;

    /* Widgets */
    GtkWidget     *root;
    GtkWidget     *stack;       /* "drop" | "viewer" */
    GtkWidget     *picture;
    GtkWidget     *scrolled;
    GtkWidget     *zoom_label;
    GtkWidget     *prev_btn;
    GtkWidget     *next_btn;
    GtkWidget     *info_label;
} ImageViewerState;

static void image_viewer_state_free(ImageViewerState *st) {
    if (!st) return;
    g_free(st->path);
    g_clear_object(&st->original);
    g_clear_object(&st->rotated);
    if (st->folder_images)
        g_ptr_array_unref(st->folder_images);
    g_free(st);
}

static ImageViewerState *get_state(GtkWidget *view) {
    return g_object_get_data(G_OBJECT(view), "image-viewer-state");
}

/* ------------------------------------------------------------------ */
/* Folder scanning                                                    */
/* ------------------------------------------------------------------ */

static gint compare_paths(gconstpointer a, gconstpointer b) {
    return g_strcmp0(*(const char **)a, *(const char **)b);
}

static void scan_folder(ImageViewerState *st, const char *file_path) {
    if (st->folder_images) {
        g_ptr_array_unref(st->folder_images);
        st->folder_images = NULL;
    }

    char *dir_path = g_path_get_dirname(file_path);
    GDir *dir = g_dir_open(dir_path, 0, NULL);
    if (!dir) {
        g_free(dir_path);
        return;
    }

    st->folder_images = g_ptr_array_new_with_free_func(g_free);

    const char *name;
    while ((name = g_dir_read_name(dir))) {
        if (!image_is_supported(name)) continue;
        char *full = g_build_filename(dir_path, name, NULL);
        g_ptr_array_add(st->folder_images, full);
    }
    g_dir_close(dir);
    g_free(dir_path);

    g_ptr_array_sort(st->folder_images, compare_paths);

    /* Find current index */
    st->current_index = -1;
    for (guint i = 0; i < st->folder_images->len; i++) {
        if (g_strcmp0(g_ptr_array_index(st->folder_images, i), file_path) == 0) {
            st->current_index = (int)i;
            break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Image loading                                                      */
/* ------------------------------------------------------------------ */

typedef struct {
    char *path;
    ImageViewerState *st;
} LoadContext;

static void load_context_free(LoadContext *ctx) {
    g_free(ctx->path);
    g_free(ctx);
}

static void load_worker(GTask *task, gpointer source_object,
                        gpointer task_data, GCancellable *cancellable) {
    (void)source_object;
    LoadContext *ctx = task_data;

    if (g_cancellable_is_cancelled(cancellable)) {
        g_task_return_new_error(task, G_IO_ERROR, G_IO_ERROR_CANCELLED,
                                "Cancelled.");
        return;
    }

    GError *error = NULL;
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(ctx->path, &error);

    if (!pixbuf) {
        g_task_return_error(task, error);
        return;
    }

    g_task_return_pointer(task, pixbuf, g_object_unref);
}

static void render_current(ImageViewerState *st);

static void on_load_finished(GObject *source, GAsyncResult *result,
                             gpointer user_data) {
    (void)source;
    GtkWidget *view = user_data;
    ImageViewerState *st = get_state(view);

    GError *error = NULL;
    GdkPixbuf *pixbuf = g_task_propagate_pointer(G_TASK(result), &error);

    if (error) {
        if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            image_show_error(view, error->message);
        g_error_free(error);
        return;
    }

    g_clear_object(&st->original);
    st->original = pixbuf;
    st->rotation = 0;
    st->fit_mode = TRUE;
    st->zoom = 0;

    render_current(st);

    /* Switch stack to viewer */
    gtk_stack_set_visible_child_name(GTK_STACK(st->stack), "viewer");

    /* Update info */
    int w = gdk_pixbuf_get_width(pixbuf);
    int h = gdk_pixbuf_get_height(pixbuf);
    char *info = g_strdup_printf("%d × %d", w, h);
    gtk_label_set_text(GTK_LABEL(st->info_label), info);
    g_free(info);

    /* Update nav buttons */
    gtk_widget_set_sensitive(st->prev_btn,
        st->current_index > 0);
    gtk_widget_set_sensitive(st->next_btn,
        st->current_index >= 0 &&
        st->current_index < (int)st->folder_images->len - 1);
}

static void load_image(ImageViewerState *st, const char *path) {
    g_free(st->path);
    st->path = g_strdup(path);

    scan_folder(st, st->path);

    LoadContext *ctx = g_new0(LoadContext, 1);
    ctx->path = g_strdup(st->path);
    ctx->st = st;

    GTask *task = g_task_new(NULL, NULL, on_load_finished, st->root);
    g_task_set_task_data(task, ctx, (GDestroyNotify)load_context_free);
    g_task_run_in_thread(task, load_worker);
    g_object_unref(task);
}

/* ------------------------------------------------------------------ */
/* Rendering                                                          */
/* ------------------------------------------------------------------ */

static void render_current(ImageViewerState *st) {
    if (!st->original) return;

    /* Apply rotation if needed */
    GdkPixbuf *source = st->original;
    if (st->rotation != 0) {
        GdkPixbuf *rot = gdk_pixbuf_rotate_simple(
            st->original,
            st->rotation == 90  ? GDK_PIXBUF_ROTATE_CLOCKWISE :
            st->rotation == 180 ? GDK_PIXBUF_ROTATE_UPSIDEDOWN :
            st->rotation == 270 ? GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE :
            GDK_PIXBUF_ROTATE_NONE);
        g_clear_object(&st->rotated);
        st->rotated = rot;
        source = rot;
    } else {
        source = st->original;
    }

    int w = gdk_pixbuf_get_width(source);
    int h = gdk_pixbuf_get_height(source);

    GdkTexture *texture = gdk_texture_new_for_pixbuf(source);
    gtk_picture_set_paintable(GTK_PICTURE(st->picture),
                              GDK_PAINTABLE(texture));
    g_object_unref(texture);

    if (st->fit_mode) {
        gtk_picture_set_can_shrink(GTK_PICTURE(st->picture), TRUE);
        gtk_picture_set_content_fit(GTK_PICTURE(st->picture),
                                     GTK_CONTENT_FIT_CONTAIN);
        gtk_widget_set_size_request(st->picture, -1, -1);
        gtk_label_set_text(GTK_LABEL(st->zoom_label), "Fit");
    } else {
        gtk_picture_set_can_shrink(GTK_PICTURE(st->picture), FALSE);
        gtk_picture_set_content_fit(GTK_PICTURE(st->picture),
                                     GTK_CONTENT_FIT_FILL);
        int scaled_w = (int)(w * st->zoom);
        int scaled_h = (int)(h * st->zoom);
        gtk_widget_set_size_request(st->picture, scaled_w, scaled_h);
        char *label = g_strdup_printf("%d%%", (int)(st->zoom * 100));
        gtk_label_set_text(GTK_LABEL(st->zoom_label), label);
        g_free(label);
    }

    /* Update nav sensitivity */
    if (st->folder_images) {
        gtk_widget_set_sensitive(st->prev_btn, st->current_index > 0);
        gtk_widget_set_sensitive(st->next_btn,
            st->current_index >= 0 &&
            st->current_index < (int)st->folder_images->len - 1);
    }
}

/* ------------------------------------------------------------------ */
/* Commands                                                           */
/* ------------------------------------------------------------------ */

typedef struct { GtkWidget *view; } OpenCtx;
static void on_open_dialog_finished(GObject *source, GAsyncResult *result, gpointer data) {
    OpenCtx *c = data;
    GtkWidget *v = c->view;
    ImageViewerState *s = get_state(v);
    GError *err = NULL;
    GFile *file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source), result, &err);
    if (err) {
        if (!g_error_matches(err, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
            image_show_error(v, err->message);
        g_error_free(err);
        g_free(c);
        return;
    }
    char *path = g_file_get_path(file);
    g_object_unref(file);
    if (path && image_is_supported(path))
        load_image(s, path);
    g_free(path);
    g_free(c);
}

static void cmd_open(GtkWidget *view) {
    ImageViewerState *st = get_state(view);

    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Open image");
    GtkFileFilter *filter = image_filter_images();
    GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
    g_list_store_append(filters, filter);
    gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));

    /* The dialog needs a completion callback. We'll route through
       a small trampoline. */

    OpenCtx *ctx = g_new0(OpenCtx, 1);
    ctx->view = view;

    GAsyncReadyCallback cb = on_open_dialog_finished;

    gtk_file_dialog_open(dialog, GTK_WINDOW(gtk_widget_get_root(view)),
                         NULL, cb, ctx);
    g_object_unref(dialog);
    g_object_unref(filters);
}

static void cmd_zoom_in(GtkWidget *view) {
    ImageViewerState *st = get_state(view);
    if (!st->original) return;
    if (st->fit_mode) {
        /* Switch to 100% first */
        st->fit_mode = FALSE;
        st->zoom = 1.0;
    } else {
        st->zoom *= 1.25;
        if (st->zoom > 10.0) st->zoom = 10.0;
    }
    render_current(st);
}

static void cmd_zoom_out(GtkWidget *view) {
    ImageViewerState *st = get_state(view);
    if (!st->original) return;
    if (st->fit_mode) return;   /* already at fit, no smaller */
    st->zoom /= 1.25;
    if (st->zoom < 0.1) st->zoom = 0.1;
    render_current(st);
}

static void cmd_zoom_reset(GtkWidget *view) {
    ImageViewerState *st = get_state(view);
    if (!st->original) return;
    st->fit_mode = FALSE;
    st->zoom = 1.0;
    render_current(st);
}

static void cmd_fit(GtkWidget *view) {
    ImageViewerState *st = get_state(view);
    if (!st->original) return;
    st->fit_mode = TRUE;
    st->zoom = 0;
    render_current(st);
}

static void cmd_rotate_cw(GtkWidget *view) {
    ImageViewerState *st = get_state(view);
    if (!st->original) return;
    st->rotation = (st->rotation + 90) % 360;
    render_current(st);
}

static void cmd_rotate_ccw(GtkWidget *view) {
    ImageViewerState *st = get_state(view);
    if (!st->original) return;
    st->rotation = (st->rotation + 270) % 360;
    render_current(st);
}

static void cmd_prev(GtkWidget *view) {
    ImageViewerState *st = get_state(view);
    if (!st->folder_images || st->current_index <= 0) return;
    st->current_index--;
    const char *path = g_ptr_array_index(st->folder_images, st->current_index);
    load_image(st, path);
}

static void cmd_next(GtkWidget *view) {
    ImageViewerState *st = get_state(view);
    if (!st->folder_images) return;
    if (st->current_index < 0 ||
        st->current_index >= (int)st->folder_images->len - 1) return;
    st->current_index++;
    const char *path = g_ptr_array_index(st->folder_images, st->current_index);
    load_image(st, path);
}

static void cmd_reset(GtkWidget *view) {
    ImageViewerState *st = get_state(view);
    g_clear_object(&st->original);
    g_clear_object(&st->rotated);
    g_clear_pointer(&st->path, g_free);
    if (st->folder_images) {
        g_ptr_array_unref(st->folder_images);
        st->folder_images = NULL;
    }
    st->current_index = -1;
    st->rotation = 0;
    st->zoom = 0;
    st->fit_mode = TRUE;

    gtk_stack_set_visible_child_name(GTK_STACK(st->stack), "drop");
}

/* ------------------------------------------------------------------ */
/* Keyboard handling                                                  */
/* ------------------------------------------------------------------ */

static gboolean on_key_pressed(GtkEventControllerKey *ctrl, guint keyval,
                               guint keycode, GdkModifierType state,
                               gpointer user_data) {
    (void)ctrl; (void)keycode; (void)state;
    GtkWidget *view = user_data;
    ImageViewerState *st = get_state(view);
    if (!st->original) return FALSE;

    switch (keyval) {
        case GDK_KEY_plus:
        case GDK_KEY_equal:
        case GDK_KEY_KP_Add:
            cmd_zoom_in(view);
            return TRUE;
        case GDK_KEY_minus:
        case GDK_KEY_KP_Subtract:
            cmd_zoom_out(view);
            return TRUE;
        case GDK_KEY_0:
        case GDK_KEY_KP_0:
            cmd_zoom_reset(view);
            return TRUE;
        case GDK_KEY_f:
        case GDK_KEY_F:
            cmd_fit(view);
            return TRUE;
        case GDK_KEY_Left:
        case GDK_KEY_Up:
            cmd_prev(view);
            return TRUE;
        case GDK_KEY_Right:
        case GDK_KEY_Down:
            cmd_next(view);
            return TRUE;
        case GDK_KEY_bracketleft:
            cmd_rotate_ccw(view);
            return TRUE;
        case GDK_KEY_bracketright:
            cmd_rotate_cw(view);
            return TRUE;
    }
    return FALSE;
}

/* ------------------------------------------------------------------ */
/* Callbacks                                                          */
/* ------------------------------------------------------------------ */

static void on_drop_file(const char *path, gpointer user_data) {
    GtkWidget *view = user_data;
    ImageViewerState *st = get_state(view);
    load_image(st, path);
}

static void on_prev_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    cmd_prev(user_data);
}

static void on_next_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    cmd_next(user_data);
}

static void on_zoom_in_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    cmd_zoom_in(user_data);
}

static void on_zoom_out_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    cmd_zoom_out(user_data);
}

static void on_fit_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    cmd_fit(user_data);
}

static void on_rotate_cw_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    cmd_rotate_cw(user_data);
}

static void on_rotate_ccw_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    cmd_rotate_ccw(user_data);
}

/* ------------------------------------------------------------------ */
/* View builder                                                       */
/* ------------------------------------------------------------------ */

static GtkWidget *build_viewer_ui(ImageViewerState *st) {
    GtkWidget *viewer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    /* ---- Toolbar ---- */
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_margin_start(toolbar, 12);
    gtk_widget_set_margin_end(toolbar, 12);
    gtk_widget_set_margin_top(toolbar, 8);
    gtk_widget_set_margin_bottom(toolbar, 8);

    GtkWidget *prev_btn = gtk_button_new_from_icon_name("go-previous-symbolic");
    GtkWidget *next_btn = gtk_button_new_from_icon_name("go-next-symbolic");
    gtk_widget_add_css_class(prev_btn, "flat");
    gtk_widget_add_css_class(next_btn, "flat");
    gtk_widget_set_tooltip_text(prev_btn, "Previous image (Left arrow)");
    gtk_widget_set_tooltip_text(next_btn, "Next image (Right arrow)");
    st->prev_btn = prev_btn;
    st->next_btn = next_btn;

    GtkWidget *zoom_out = gtk_button_new_from_icon_name("zoom-out-symbolic");
    GtkWidget *zoom_in  = gtk_button_new_from_icon_name("zoom-in-symbolic");
    GtkWidget *fit_btn  = gtk_button_new_from_icon_name("zoom-fit-best-symbolic");
    GtkWidget *rot_cw   = gtk_button_new_from_icon_name("object-rotate-right-symbolic");
    GtkWidget *rot_ccw  = gtk_button_new_from_icon_name("object-rotate-left-symbolic");
    gtk_widget_add_css_class(zoom_out, "flat");
    gtk_widget_add_css_class(zoom_in, "flat");
    gtk_widget_add_css_class(fit_btn, "flat");
    gtk_widget_add_css_class(rot_cw, "flat");
    gtk_widget_add_css_class(rot_ccw, "flat");
    gtk_widget_set_tooltip_text(zoom_out, "Zoom out (−)");
    gtk_widget_set_tooltip_text(zoom_in, "Zoom in (+)");
    gtk_widget_set_tooltip_text(fit_btn, "Fit to window (F)");
    gtk_widget_set_tooltip_text(rot_cw, "Rotate clockwise (])");
    gtk_widget_set_tooltip_text(rot_ccw, "Rotate counter-clockwise ([)");

    GtkWidget *zoom_label = gtk_label_new("Fit");
    gtk_widget_add_css_class(zoom_label, "dim-label");
    gtk_widget_set_size_request(zoom_label, 60, -1);
    st->zoom_label = zoom_label;

    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(spacer, TRUE);

    GtkWidget *info_label = gtk_label_new("");
    gtk_widget_add_css_class(info_label, "dim-label");
    gtk_widget_add_css_class(info_label, "caption");
    st->info_label = info_label;

    gtk_box_append(GTK_BOX(toolbar), prev_btn);
    gtk_box_append(GTK_BOX(toolbar), next_btn);
    gtk_box_append(GTK_BOX(toolbar), gtk_separator_new(GTK_ORIENTATION_VERTICAL));
    gtk_box_append(GTK_BOX(toolbar), zoom_out);
    gtk_box_append(GTK_BOX(toolbar), zoom_label);
    gtk_box_append(GTK_BOX(toolbar), zoom_in);
    gtk_box_append(GTK_BOX(toolbar), fit_btn);
    gtk_box_append(GTK_BOX(toolbar), gtk_separator_new(GTK_ORIENTATION_VERTICAL));
    gtk_box_append(GTK_BOX(toolbar), rot_ccw);
    gtk_box_append(GTK_BOX(toolbar), rot_cw);
    gtk_box_append(GTK_BOX(toolbar), gtk_separator_new(GTK_ORIENTATION_VERTICAL));
    gtk_box_append(GTK_BOX(toolbar), image_new_image_button(on_drop_file, st->root));
    gtk_box_append(GTK_BOX(toolbar), spacer);
    gtk_box_append(GTK_BOX(toolbar), info_label);

    /* ---- Canvas ---- */
    GtkWidget *picture = gtk_picture_new();
    gtk_widget_set_halign(picture, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(picture, GTK_ALIGN_CENTER);
    gtk_picture_set_can_shrink(GTK_PICTURE(picture), TRUE);
    gtk_picture_set_content_fit(GTK_PICTURE(picture), GTK_CONTENT_FIT_CONTAIN);
    st->picture = picture;

    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), picture);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_widget_set_hexpand(scrolled, TRUE);
    gtk_widget_add_css_class(scrolled, "image-viewer-canvas");
    st->scrolled = scrolled;

    gtk_box_append(GTK_BOX(viewer), toolbar);
    gtk_box_append(GTK_BOX(viewer), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(viewer), scrolled);

    /* Wire buttons */
    g_signal_connect(prev_btn,   "clicked", G_CALLBACK(on_prev_clicked),   st->root);
    g_signal_connect(next_btn,   "clicked", G_CALLBACK(on_next_clicked),   st->root);
    g_signal_connect(zoom_in,    "clicked", G_CALLBACK(on_zoom_in_clicked), st->root);
    g_signal_connect(zoom_out,   "clicked", G_CALLBACK(on_zoom_out_clicked), st->root);
    g_signal_connect(fit_btn,    "clicked", G_CALLBACK(on_fit_clicked),    st->root);
    g_signal_connect(rot_cw,     "clicked", G_CALLBACK(on_rotate_cw_clicked), st->root);
    g_signal_connect(rot_ccw,    "clicked", G_CALLBACK(on_rotate_ccw_clicked), st->root);

    return viewer;
}

/* ------------------------------------------------------------------ */
/* Lifecycle                                                          */
/* ------------------------------------------------------------------ */

void image_viewer_on_close(GtkWidget *view) {
    ImageViewerState *st = get_state(view);
    image_viewer_state_free(st);
    g_object_set_data(G_OBJECT(view), "image-viewer-state", NULL);
}

/* ------------------------------------------------------------------ */
/* Create view                                                        */
/* ------------------------------------------------------------------ */

GtkWidget *image_viewer_create(void) {
    ImageViewerState *st = g_new0(ImageViewerState, 1);
    st->rotation = 0;
    st->fit_mode = TRUE;
    st->current_index = -1;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    gtk_widget_set_hexpand(root, TRUE);
    st->root = root;

    GtkWidget *stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(stack),
                                   GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration(GTK_STACK(stack), 150);
    gtk_widget_set_vexpand(stack, TRUE);
    gtk_widget_set_hexpand(stack, TRUE);
    st->stack = stack;

    /* Drop zone page */
    GtkWidget *drop_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(drop_container, 24);
    gtk_widget_set_margin_end(drop_container, 24);
    gtk_widget_set_margin_top(drop_container, 24);
    gtk_widget_set_margin_bottom(drop_container, 24);
    gtk_widget_set_vexpand(drop_container, TRUE);

    GtkWidget *drop = image_build_drop_zone(
        "Image file (PNG, JPG, WEBP, AVIF, HEIC, SVG, RAW)",
        on_drop_file, root);
    gtk_box_append(GTK_BOX(drop_container), drop);

    gtk_stack_add_named(GTK_STACK(stack), drop_container, "drop");

    /* Viewer page */
    GtkWidget *viewer = build_viewer_ui(st);
    gtk_stack_add_named(GTK_STACK(stack), viewer, "viewer");

    gtk_stack_set_visible_child_name(GTK_STACK(stack), "drop");

    gtk_box_append(GTK_BOX(root), stack);

    /* Store state */
    g_object_set_data_full(G_OBJECT(root), "image-viewer-state", st,
                           (GDestroyNotify)image_viewer_state_free);

    /* Keyboard controller */
    GtkEventController *key = gtk_event_controller_key_new();
    g_signal_connect(key, "key-pressed", G_CALLBACK(on_key_pressed), root);
    gtk_widget_add_controller(root, key);

    return root;
}

/* ------------------------------------------------------------------ */
/* Command table                                                      */
/* ------------------------------------------------------------------ */

const HelvetiaToolCommand image_viewer_commands[] = {
    { .id = "open", .name = "Open…",
      .icon_name = "document-open-symbolic",
      .accel = "<Control>o", .tooltip = "Open an image",
      .activate = cmd_open },
    { .id = "prev", .name = "Previous",
      .icon_name = "go-previous-symbolic",
      .accel = NULL, .tooltip = "Previous image",
      .activate = cmd_prev },
    { .id = "next", .name = "Next",
      .icon_name = "go-next-symbolic",
      .accel = NULL, .tooltip = "Next image",
      .activate = cmd_next },
    { .id = "zoom-in", .name = "Zoom In",
      .icon_name = "zoom-in-symbolic",
      .accel = "<Control>plus", .tooltip = "Zoom in",
      .activate = cmd_zoom_in },
    { .id = "zoom-out", .name = "Zoom Out",
      .icon_name = "zoom-out-symbolic",
      .accel = "<Control>minus", .tooltip = "Zoom out",
      .activate = cmd_zoom_out },
    { .id = "zoom-reset", .name = "Actual Size",
      .icon_name = "zoom-original-symbolic",
      .accel = "<Control>0", .tooltip = "100% zoom",
      .activate = cmd_zoom_reset },
    { .id = "fit", .name = "Fit to Window",
      .icon_name = "zoom-fit-best-symbolic",
      .accel = "<Control>f", .tooltip = "Fit image in window",
      .activate = cmd_fit },
    { .id = "rotate-cw", .name = "Rotate Clockwise",
      .icon_name = "object-rotate-right-symbolic",
      .accel = NULL, .tooltip = "Rotate 90° clockwise",
      .activate = cmd_rotate_cw },
    { .id = "rotate-ccw", .name = "Rotate Counter-clockwise",
      .icon_name = "object-rotate-left-symbolic",
      .accel = NULL, .tooltip = "Rotate 90° counter-clockwise",
      .activate = cmd_rotate_ccw },
    { .id = "reset", .name = "Close Image",
      .icon_name = "window-close-symbolic",
      .accel = NULL, .tooltip = "Close the current image",
      .activate = cmd_reset },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};
