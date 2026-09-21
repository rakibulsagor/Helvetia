#include <gtk/gtk.h>
#include <adwaita.h>
#include "../image_shared.h"
#include "image_slideshow.h"

typedef struct {
    GPtrArray *images;
    int        current;
    gboolean   playing;
    guint      timer_id;
    guint      interval;
    gboolean   loop_enabled;

    GtkWidget *stack, *picture, *counter;
    GtkWidget *play_btn, *prev_btn, *next_btn, *fullscreen_btn;
    GtkWidget *root;
} SlideshowState;

static void state_free(SlideshowState *st) {
    if (!st) return;
    if (st->timer_id) g_source_remove(st->timer_id);
    if (st->images) g_ptr_array_unref(st->images);
    g_free(st);
}

static SlideshowState *get_state(GtkWidget *v) {
    return g_object_get_data(G_OBJECT(v), "slideshow-state");
}

static void render_current(SlideshowState *st) {
    if (!st->images || st->images->len == 0) return;
    if (st->current < 0) st->current = 0;
    if (st->current >= (int)st->images->len) st->current = 0;

    const char *path = g_ptr_array_index(st->images, st->current);
    GError *e = NULL;
    GdkPixbuf *pb = gdk_pixbuf_new_from_file(path, &e);
    if (!pb) { image_show_error(st->root, e->message); g_error_free(e); return; }

    GdkTexture *t = gdk_texture_new_for_pixbuf(pb);
    gtk_picture_set_paintable(GTK_PICTURE(st->picture), GDK_PAINTABLE(t));
    g_object_unref(t);
    g_object_unref(pb);

    char *lbl = g_strdup_printf("%d / %u", st->current + 1, st->images->len);
    gtk_label_set_text(GTK_LABEL(st->counter), lbl);
    g_free(lbl);

    gtk_widget_set_sensitive(st->prev_btn, st->current > 0 || st->loop_enabled);
    gtk_widget_set_sensitive(st->next_btn,
        st->current + 1 < (int)st->images->len || st->loop_enabled);
}

static void slideshow_stop(SlideshowState *st) {
    if (st->timer_id) { g_source_remove(st->timer_id); st->timer_id = 0; }
    st->playing = FALSE;
    gtk_button_set_icon_name(GTK_BUTTON(st->play_btn),
                             "media-playback-start-symbolic");
}

static gboolean tick(gpointer d) {
    SlideshowState *st = d;
    if (!st->playing) {
        st->timer_id = 0;
        return G_SOURCE_REMOVE;
    }
    if (st->current + 1 < (int)st->images->len) st->current++;
    else if (st->loop_enabled) st->current = 0;
    else {
        st->playing = FALSE;
        gtk_button_set_icon_name(GTK_BUTTON(st->play_btn),
                                 "media-playback-start-symbolic");
        st->timer_id = 0;
        return G_SOURCE_REMOVE;
    }
    render_current(st);
    return G_SOURCE_CONTINUE;
}

static void slideshow_start(SlideshowState *st) {
    if (st->playing || !st->images || st->images->len < 2) return;
    st->playing = TRUE;
    gtk_button_set_icon_name(GTK_BUTTON(st->play_btn),
                             "media-playback-pause-symbolic");
    st->timer_id = g_timeout_add_seconds(st->interval, tick, st);
}

static void slideshow_toggle(SlideshowState *st) {
    if (st->playing) slideshow_stop(st);
    else slideshow_start(st);
}

static void go_next(SlideshowState *st) {
    if (!st->images || st->images->len == 0) return;
    if (st->current + 1 < (int)st->images->len) st->current++;
    else if (st->loop_enabled) st->current = 0;
    else return;
    render_current(st);
}

static void go_prev(SlideshowState *st) {
    if (!st->images || st->images->len == 0) return;
    if (st->current > 0) st->current--;
    else if (st->loop_enabled) st->current = st->images->len - 1;
    else return;
    render_current(st);
}

static gint cmp_str(gconstpointer a, gconstpointer b) {
    return g_strcmp0(*(const char **)a, *(const char **)b);
}

static void load_folder(SlideshowState *st, const char *file_path) {
    char *dir_path = g_path_get_dirname(file_path);
    GDir *dir = g_dir_open(dir_path, 0, NULL);
    if (!dir) { g_free(dir_path); return; }

    if (st->images) g_ptr_array_unref(st->images);
    st->images = g_ptr_array_new_with_free_func(g_free);

    const char *name;
    while ((name = g_dir_read_name(dir))) {
        char *full = g_build_filename(dir_path, name, NULL);
        if (image_is_supported(full)) g_ptr_array_add(st->images, full);
        else g_free(full);
    }
    g_dir_close(dir);
    g_free(dir_path);

    if (st->images->len == 0) {
        image_show_error(st->root, "No images found in folder");
        return;
    }

    g_ptr_array_sort(st->images, cmp_str);

    st->current = 0;
    for (guint i = 0; i < st->images->len; i++) {
        if (g_strcmp0(g_ptr_array_index(st->images, i), file_path) == 0) {
            st->current = (int)i;
            break;
        }
    }

    render_current(st);
    gtk_stack_set_visible_child_name(GTK_STACK(st->stack), "viewer");
}

static void toggle_fullscreen(SlideshowState *st) {
    GtkRoot *r = gtk_widget_get_root(st->root);
    if (!r || !GTK_IS_WINDOW(r)) return;
    GtkWindow *w = GTK_WINDOW(r);
    if (gtk_window_is_fullscreen(w)) {
        gtk_window_unfullscreen(w);
        gtk_button_set_icon_name(GTK_BUTTON(st->fullscreen_btn),
                                 "view-fullscreen-symbolic");
    } else {
        gtk_window_fullscreen(w);
        gtk_button_set_icon_name(GTK_BUTTON(st->fullscreen_btn),
                                 "view-restore-symbolic");
    }
}

static void on_drop(const char *path, gpointer d) {
    load_folder(get_state(d), path);
}
static void on_prev(GtkButton *b, gpointer d) { (void)b; go_prev(get_state(d)); }
static void on_next(GtkButton *b, gpointer d) { (void)b; go_next(get_state(d)); }
static void on_play(GtkButton *b, gpointer d) { (void)b; slideshow_toggle(get_state(d)); }
static void on_fs  (GtkButton *b, gpointer d) { (void)b; toggle_fullscreen(get_state(d)); }

static void on_interval(GObject *dd, GParamSpec *p, gpointer d) {
    (void)p;
    SlideshowState *st = get_state(d);
    static const guint s[] = {1, 2, 3, 5, 10, 30};
    guint i = gtk_drop_down_get_selected(GTK_DROP_DOWN(dd));
    if (i >= G_N_ELEMENTS(s)) i = 2;
    st->interval = s[i];
    if (st->playing) { slideshow_stop(st); slideshow_start(st); }
}

static void on_loop(GObject *sw, GParamSpec *p, gpointer d) {
    (void)p;
    SlideshowState *st = get_state(d);
    st->loop_enabled = gtk_switch_get_active(GTK_SWITCH(sw));
    render_current(st);
}

static gboolean on_key(GtkEventControllerKey *c, guint key,
                       guint code, GdkModifierType mod, gpointer d) {
    (void)c; (void)code; (void)mod;
    SlideshowState *st = get_state(d);
    switch (key) {
        case GDK_KEY_space: slideshow_toggle(st); return TRUE;
        case GDK_KEY_Left:  go_prev(st); return TRUE;
        case GDK_KEY_Right: go_next(st); return TRUE;
        case GDK_KEY_F11:   toggle_fullscreen(st); return TRUE;
        case GDK_KEY_Escape: {
            GtkRoot *r = gtk_widget_get_root(st->root);
            if (r && GTK_IS_WINDOW(r) && gtk_window_is_fullscreen(GTK_WINDOW(r))) {
                gtk_window_unfullscreen(GTK_WINDOW(r));
                return TRUE;
            }
            return FALSE;
        }
    }
    return FALSE;
}

void image_slideshow_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "slideshow-state", NULL);
}

static void cmd_play(GtkWidget *v) { slideshow_toggle(get_state(v)); }
static void cmd_next(GtkWidget *v) { go_next(get_state(v)); }
static void cmd_prev(GtkWidget *v) { go_prev(get_state(v)); }
static void cmd_fs(GtkWidget *v)   { toggle_fullscreen(get_state(v)); }

const HelvetiaToolCommand image_slideshow_commands[] = {
    { .id = "play", .name = "Play / Pause",
      .icon_name = "media-playback-start-symbolic",
      .accel = "space", .tooltip = "Toggle playback", .activate = cmd_play },
    { .id = "next", .name = "Next", .icon_name = "go-next-symbolic",
      .accel = NULL, .tooltip = "Next image", .activate = cmd_next },
    { .id = "prev", .name = "Previous", .icon_name = "go-previous-symbolic",
      .accel = NULL, .tooltip = "Previous image", .activate = cmd_prev },
    { .id = "fullscreen", .name = "Fullscreen",
      .icon_name = "view-fullscreen-symbolic",
      .accel = "F11", .tooltip = "Toggle fullscreen", .activate = cmd_fs },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

GtkWidget *image_slideshow_create(void) {
    SlideshowState *st = g_new0(SlideshowState, 1);
    st->interval = 3;
    st->current = 0;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    gtk_widget_set_hexpand(root, TRUE);
    st->root = root;

    GtkWidget *stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(stack),
                                  GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration(GTK_STACK(stack), 150);
    gtk_widget_set_vexpand(stack, TRUE);
    st->stack = stack;

    /* Drop page */
    GtkWidget *drop_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(drop_box, 24);
    gtk_widget_set_margin_end(drop_box, 24);
    gtk_widget_set_margin_top(drop_box, 24);
    gtk_widget_set_margin_bottom(drop_box, 24);
    gtk_widget_set_vexpand(drop_box, TRUE);
    GtkWidget *drop = image_build_drop_zone(
        "Image file (opens its folder for the slideshow)", on_drop, root);
    gtk_box_append(GTK_BOX(drop_box), drop);
    gtk_stack_add_named(GTK_STACK(stack), drop_box, "drop");

    /* Viewer page */
    GtkWidget *viewer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    GtkWidget *picture = gtk_picture_new();
    gtk_picture_set_can_shrink(GTK_PICTURE(picture), TRUE);
    gtk_picture_set_content_fit(GTK_PICTURE(picture), GTK_CONTENT_FIT_CONTAIN);
    gtk_widget_set_vexpand(picture, TRUE);
    gtk_widget_set_hexpand(picture, TRUE);
    gtk_widget_add_css_class(picture, "image-viewer-canvas");
    st->picture = picture;

    GtkWidget *controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(controls, 12);
    gtk_widget_set_margin_end(controls, 12);
    gtk_widget_set_margin_top(controls, 8);
    gtk_widget_set_margin_bottom(controls, 8);

    GtkWidget *prev = gtk_button_new_from_icon_name("go-previous-symbolic");
    GtkWidget *play = gtk_button_new_from_icon_name("media-playback-start-symbolic");
    GtkWidget *next = gtk_button_new_from_icon_name("go-next-symbolic");
    GtkWidget *fs   = gtk_button_new_from_icon_name("view-fullscreen-symbolic");
    gtk_widget_add_css_class(prev, "flat");
    gtk_widget_add_css_class(play, "flat");
    gtk_widget_add_css_class(next, "flat");
    gtk_widget_add_css_class(fs, "flat");
    st->prev_btn = prev; st->play_btn = play;
    st->next_btn = next; st->fullscreen_btn = fs;

    GtkWidget *counter = gtk_label_new("0 / 0");
    gtk_widget_add_css_class(counter, "dim-label");
    gtk_widget_set_size_request(counter, 80, -1);
    st->counter = counter;

    const char *iv[] = {"1 s", "2 s", "3 s", "5 s", "10 s", "30 s", NULL};
    GtkWidget *interval = gtk_drop_down_new_from_strings(iv);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(interval), 2);

    GtkWidget *loop_lbl = gtk_label_new("Loop");
    gtk_widget_add_css_class(loop_lbl, "dim-label");
    GtkWidget *loop_sw = gtk_switch_new();
    gtk_widget_set_valign(loop_sw, GTK_ALIGN_CENTER);

    GtkWidget *sp1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(sp1, TRUE);
    GtkWidget *sp2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(sp2, TRUE);

    gtk_box_append(GTK_BOX(controls), sp1);
    gtk_box_append(GTK_BOX(controls), prev);
    gtk_box_append(GTK_BOX(controls), play);
    gtk_box_append(GTK_BOX(controls), next);
    gtk_box_append(GTK_BOX(controls), gtk_separator_new(GTK_ORIENTATION_VERTICAL));
    gtk_box_append(GTK_BOX(controls), counter);
    gtk_box_append(GTK_BOX(controls), gtk_separator_new(GTK_ORIENTATION_VERTICAL));
    gtk_box_append(GTK_BOX(controls), interval);
    gtk_box_append(GTK_BOX(controls), loop_lbl);
    gtk_box_append(GTK_BOX(controls), loop_sw);
    gtk_box_append(GTK_BOX(controls), gtk_separator_new(GTK_ORIENTATION_VERTICAL));
    gtk_box_append(GTK_BOX(controls), fs);
    gtk_box_append(GTK_BOX(controls), gtk_separator_new(GTK_ORIENTATION_VERTICAL));
    gtk_box_append(GTK_BOX(controls), image_new_image_button(on_drop, root));
    gtk_box_append(GTK_BOX(controls), sp2);

    gtk_box_append(GTK_BOX(viewer), picture);
    gtk_box_append(GTK_BOX(viewer), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(viewer), controls);

    gtk_stack_add_named(GTK_STACK(stack), viewer, "viewer");
    gtk_stack_set_visible_child_name(GTK_STACK(stack), "drop");

    gtk_box_append(GTK_BOX(root), stack);

    g_object_set_data_full(G_OBJECT(root), "slideshow-state", st,
                           (GDestroyNotify)state_free);

    g_signal_connect(prev, "clicked", G_CALLBACK(on_prev), root);
    g_signal_connect(next, "clicked", G_CALLBACK(on_next), root);
    g_signal_connect(play, "clicked", G_CALLBACK(on_play), root);
    g_signal_connect(fs,   "clicked", G_CALLBACK(on_fs), root);
    g_signal_connect(interval, "notify::selected", G_CALLBACK(on_interval), root);
    g_signal_connect(loop_sw, "notify::active", G_CALLBACK(on_loop), root);

    GtkEventController *key = gtk_event_controller_key_new();
    g_signal_connect(key, "key-pressed", G_CALLBACK(on_key), root);
    gtk_widget_add_controller(root, key);

    return root;
}
