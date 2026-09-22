#include "image_shared.h"
#include <string.h>
#include <glib/gstdio.h>

/* ------------------------------------------------------------------ */
/* Filters                                                            */
/* ------------------------------------------------------------------ */

GtkFileFilter *image_filter_images(void) {
    GtkFileFilter *f = gtk_file_filter_new();
    gtk_file_filter_set_name(f, "Images");
    gtk_file_filter_add_mime_type(f, "image/*");

    const char *patterns[] = {
        "*.png", "*.jpg", "*.jpeg", "*.jpe",
        "*.gif", "*.webp", "*.bmp", "*.tiff", "*.tif",
        "*.svg", "*.avif", "*.heic", "*.heif", "*.ico",
        "*.ppm", "*.pgm", "*.pbm", "*.pnm", "*.tga",
        "*.dng", "*.cr2", "*.cr3", "*.nef", "*.arw",
        "*.raf", "*.orf", "*.rw2", "*.pef", "*.srw",
        NULL
    };
    for (int i = 0; patterns[i]; i++)
        gtk_file_filter_add_pattern(f, patterns[i]);

    return f;
}

GtkFileFilter *image_filter_all(void) {
    GtkFileFilter *f = gtk_file_filter_new();
    gtk_file_filter_set_name(f, "All files");
    gtk_file_filter_add_pattern(f, "*");
    return f;
}

GListStore *image_filter_store_full(void) {
    GListStore *store = g_list_store_new(GTK_TYPE_FILE_FILTER);
    g_list_store_append(store, image_filter_images());
    g_list_store_append(store, image_filter_all());
    return store;
}

/* ------------------------------------------------------------------ */
/* Extension helpers                                                  */
/* ------------------------------------------------------------------ */

char *image_get_extension(const char *path) {
    if (!path) return NULL;
    const char *dot = strrchr(path, '.');
    if (!dot || dot == path) return NULL;
    return g_utf8_strdown(dot + 1, -1);
}

gboolean image_is_supported(const char *path) {
    char *ext = image_get_extension(path);
    if (!ext) return FALSE;

    static const char *supported[] = {
        "png", "jpg", "jpeg", "jpe", "gif", "webp", "bmp",
        "tiff", "tif", "svg", "avif", "heic", "heif", "ico",
        "ppm", "pgm", "pbm", "pnm", "tga",
        "dng", "cr2", "cr3", "nef", "arw", "raf", "orf",
        "rw2", "pef", "srw",
        NULL
    };
    for (int i = 0; supported[i]; i++) {
        if (g_strcmp0(ext, supported[i]) == 0) {
            g_free(ext);
            return TRUE;
        }
    }
    g_free(ext);
    return FALSE;
}

char *image_format_size(guint64 bytes) {
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    double value = (double)bytes;
    int unit = 0;
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        unit++;
    }
    return g_strdup_printf("%.1f %s", value, units[unit]);
}

/* ------------------------------------------------------------------ */
/* Toasts                                                             */
/* ------------------------------------------------------------------ */

static AdwToastOverlay *find_toast_overlay(GtkWidget *widget) {
    GtkRoot *root = gtk_widget_get_root(widget);
    if (!root) return NULL;

    GtkWidget *child = gtk_window_get_child(GTK_WINDOW(root));
    while (child) {
        if (ADW_IS_TOAST_OVERLAY(child))
            return ADW_TOAST_OVERLAY(child);

        GtkWidget *next = NULL;
        if (ADW_IS_TOOLBAR_VIEW(child))
            next = adw_toolbar_view_get_content(ADW_TOOLBAR_VIEW(child));

        child = next ? next : gtk_widget_get_first_child(child);
    }
    return NULL;
}

void image_show_error(GtkWidget *widget, const char *message) {
    AdwToastOverlay *overlay = find_toast_overlay(widget);
    if (overlay) {
        AdwToast *toast = adw_toast_new(message);
        adw_toast_set_timeout(toast, 5);
        adw_toast_overlay_add_toast(overlay, toast);
    } else {
        g_warning("image: %s", message);
    }
}

void image_show_info(GtkWidget *widget, const char *message) {
    AdwToastOverlay *overlay = find_toast_overlay(widget);
    if (overlay) {
        AdwToast *toast = adw_toast_new(message);
        adw_toast_set_timeout(toast, 3);
        adw_toast_overlay_add_toast(overlay, toast);
    }
}

/* ------------------------------------------------------------------ */
/* Drop zone                                                          */
/* ------------------------------------------------------------------ */

typedef struct {
    ImageDropCallback on_file;
    gpointer          user_data;
} DropZoneData;

static void drop_zone_data_free(DropZoneData *d) {
    g_free(d);
}

/* Called when the user picks a file via the file dialog */
static void on_file_dialog_finished(GObject *source, GAsyncResult *result,
                                    gpointer user_data) {
    GtkWidget *widget = user_data;
    DropZoneData *d = g_object_get_data(G_OBJECT(widget), "drop-zone-data");

    GError *error = NULL;
    GFile *file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source),
                                              result, &error);
    if (error) {
        if (!g_error_matches(error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
            image_show_error(widget, error->message);
        g_error_free(error);
        return;
    }

    char *path = g_file_get_path(file);
    g_object_unref(file);

    if (path && d && d->on_file)
        d->on_file(path, d->user_data);

    g_free(path);
}

/* Clicked the drop zone */
static void on_drop_zone_clicked(GtkButton *btn, gpointer user_data) {
    (void)user_data;
    GtkWidget *widget = GTK_WIDGET(btn);

    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Open image");
    gtk_file_dialog_set_accept_label(dialog, "Open");

    GListStore *filters = image_filter_store_full();
    gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));

    gtk_file_dialog_open(dialog,
                         GTK_WINDOW(gtk_widget_get_root(widget)),
                         NULL, on_file_dialog_finished, widget);

    g_object_unref(dialog);
    g_object_unref(filters);
}

/* Drag-and-drop file received */
static gboolean on_drop(GtkDropTarget *target, const GValue *value,
                        double x, double y, gpointer user_data) {
    (void)target; (void)x; (void)y;
    GtkWidget *widget = user_data;
    DropZoneData *d = g_object_get_data(G_OBJECT(widget), "drop-zone-data");

    if (!G_VALUE_HOLDS(value, G_TYPE_FILE))
        return FALSE;

    GFile *file = g_value_get_object(value);
    if (!file) return FALSE;

    char *path = g_file_get_path(file);
    if (!path) return FALSE;

    if (!image_is_supported(path)) {
        image_show_error(widget, "Unsupported image format");
        g_free(path);
        return FALSE;
    }

    if (d && d->on_file)
        d->on_file(path, d->user_data);

    g_free(path);
    return TRUE;
}

GtkWidget *image_build_drop_zone(const char *hint_text,
                                  ImageDropCallback on_file,
                                  gpointer user_data) {
    GtkWidget *btn = gtk_button_new();
    gtk_widget_add_css_class(btn, "image-drop-zone");
    gtk_widget_set_hexpand(btn, TRUE);
    gtk_widget_set_vexpand(btn, TRUE);
    gtk_widget_set_size_request(btn, 300, 200);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_start(box, 32);
    gtk_widget_set_margin_end(box, 32);
    gtk_widget_set_margin_top(box, 32);
    gtk_widget_set_margin_bottom(box, 32);

    GtkWidget *icon = gtk_image_new_from_icon_name("image-x-generic-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 48);
    gtk_widget_add_css_class(icon, "image-drop-zone-icon");

    GtkWidget *title = gtk_label_new("Click to select a file");
    gtk_widget_add_css_class(title, "title-3");

    GtkWidget *or_label = gtk_label_new("or drag and drop");
    gtk_widget_add_css_class(or_label, "dim-label");

    GtkWidget *hint = gtk_label_new(hint_text ? hint_text : "Image file");
    gtk_widget_add_css_class(hint, "dim-label");

    GtkWidget *privacy = gtk_label_new("Your files never leave your device.");
    gtk_widget_add_css_class(privacy, "dim-label");
    gtk_widget_add_css_class(privacy, "caption");

    gtk_box_append(GTK_BOX(box), icon);
    gtk_box_append(GTK_BOX(box), title);
    gtk_box_append(GTK_BOX(box), or_label);
    gtk_box_append(GTK_BOX(box), hint);
    gtk_box_append(GTK_BOX(box), privacy);

    gtk_button_set_child(GTK_BUTTON(btn), box);

    /* Store callback data */
    DropZoneData *d = g_new0(DropZoneData, 1);
    d->on_file = on_file;
    d->user_data = user_data;
    g_object_set_data_full(G_OBJECT(btn), "drop-zone-data", d,
                           (GDestroyNotify)drop_zone_data_free);

    /* Wire click */
    g_signal_connect(btn, "clicked", G_CALLBACK(on_drop_zone_clicked), NULL);

    /* Wire drop */
    GtkDropTarget *target = gtk_drop_target_new(G_TYPE_FILE, GDK_ACTION_COPY);
    g_signal_connect(target, "drop", G_CALLBACK(on_drop), btn);
    gtk_widget_add_controller(btn, GTK_EVENT_CONTROLLER(target));

    return btn;
}

/* ------------------------------------------------------------------ */
/* Save dialog                                                        */
/* ------------------------------------------------------------------ */

typedef struct {
    GdkPixbuf *pixbuf;
} SaveCtx;

static void on_save_done(GObject *src, GAsyncResult *res, gpointer data) {
    SaveCtx *c = data;
    GtkFileDialog *dlg = GTK_FILE_DIALOG(src);
    GError *e = NULL;
    GFile *f = gtk_file_dialog_save_finish(dlg, res, &e);

    if (e) {
        if (!g_error_matches(e, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
            g_warning("save dialog: %s", e->message);
        g_error_free(e);
        goto cleanup;
    }

    char *path = g_file_get_path(f);
    g_object_unref(f);

    if (!path) { goto cleanup; }

    /* Pick format from extension */
    const char *fmt = "png";
    const char *dot = strrchr(path, '.');
    if (dot) {
        if (!g_ascii_strcasecmp(dot, ".jpg") ||
            !g_ascii_strcasecmp(dot, ".jpeg")) fmt = "jpeg";
        else if (!g_ascii_strcasecmp(dot, ".png")) fmt = "png";
        else if (!g_ascii_strcasecmp(dot, ".bmp")) fmt = "bmp";
        else if (!g_ascii_strcasecmp(dot, ".tif") ||
                 !g_ascii_strcasecmp(dot, ".tiff")) fmt = "tiff";
    } else {
        /* No extension — append .png */
        char *fixed = g_strconcat(path, ".png", NULL);
        g_free(path);
        path = fixed;
    }

    /* Ensure we save RGBA with alpha if the pixbuf has it */
    GError *err = NULL;
    gboolean ok;
    if (strcmp(fmt, "jpeg") == 0) {
        /* JPEG has no alpha */
        ok = gdk_pixbuf_save(c->pixbuf, path, "jpeg", &err,
                             "quality", "92", NULL);
    } else if (strcmp(fmt, "png") == 0) {
        ok = gdk_pixbuf_save(c->pixbuf, path, "png", &err,
                             "compression", "6", NULL);
    } else {
        ok = gdk_pixbuf_save(c->pixbuf, path, fmt, &err, NULL);
    }

    if (!ok) {
        g_warning("save failed: %s", err ? err->message : "unknown");
        if (err) g_error_free(err);
    } else {
        g_message("Saved: %s", path);
    }
    g_free(path);

cleanup:
    g_object_unref(c->pixbuf);
    g_free(c);
}

void image_save_pixbuf_dialog(GtkWidget *parent,
                               GdkPixbuf *pixbuf,
                               const char *suggested_name) {
    if (!parent || !pixbuf) return;

    GtkRoot *root = gtk_widget_get_root(parent);
    if (!root || !GTK_IS_WINDOW(root)) {
        g_warning("save: no window found");
        return;
    }

    GtkFileDialog *dlg = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dlg, "Save image");
    if (suggested_name)
        gtk_file_dialog_set_initial_name(dlg, suggested_name);

    GListStore *fs = g_list_store_new(GTK_TYPE_FILE_FILTER);

    GtkFileFilter *png = gtk_file_filter_new();
    gtk_file_filter_set_name(png, "PNG image");
    gtk_file_filter_add_pattern(png, "*.png");
    g_list_store_append(fs, png);

    GtkFileFilter *jpg = gtk_file_filter_new();
    gtk_file_filter_set_name(jpg, "JPEG image");
    gtk_file_filter_add_pattern(jpg, "*.jpg");
    gtk_file_filter_add_pattern(jpg, "*.jpeg");
    g_list_store_append(fs, jpg);

    GtkFileFilter *all = gtk_file_filter_new();
    gtk_file_filter_set_name(all, "All files");
    gtk_file_filter_add_pattern(all, "*");
    g_list_store_append(fs, all);

    gtk_file_dialog_set_filters(dlg, G_LIST_MODEL(fs));
    gtk_file_dialog_set_default_filter(dlg, png);

    SaveCtx *c = g_new0(SaveCtx, 1);
    c->pixbuf = g_object_ref(pixbuf);

    gtk_file_dialog_save(dlg, GTK_WINDOW(root), NULL, on_save_done, c);

    g_object_unref(dlg);
    g_object_unref(fs);
}

/* ------------------------------------------------------------------ */
/* New Image button — same callback as drop zone                      */
/* ------------------------------------------------------------------ */

GtkWidget *image_new_image_button(ImageDropCallback on_file,
                                   gpointer          user_data) {
    GtkWidget *btn = gtk_button_new_with_label("New Image…");
    gtk_widget_add_css_class(btn, "flat");
    gtk_widget_set_tooltip_text(btn, "Open a different image");

    DropZoneData *d = g_new0(DropZoneData, 1);
    d->on_file = on_file;
    d->user_data = user_data;
    g_object_set_data_full(G_OBJECT(btn), "drop-zone-data", d,
                           (GDestroyNotify)drop_zone_data_free);

    /* Reuse the same dialog launcher as the drop zone */
    g_signal_connect(btn, "clicked", G_CALLBACK(on_drop_zone_clicked), NULL);

    return btn;
}

/* ------------------------------------------------------------------ */
/* Ctrl+Z / Ctrl+R shortcuts                                          */
/* ------------------------------------------------------------------ */

typedef struct {
    ImageToolCallback on_undo;
    ImageToolCallback on_reset;
    gpointer          user_data;
} ShortcutData;

static gboolean on_shortcut_key(GtkEventControllerKey *ctrl,
                                 guint keyval, guint keycode,
                                 GdkModifierType mods, gpointer user_data) {
    (void)ctrl; (void)keycode;
    ShortcutData *d = user_data;

    if (!(mods & GDK_CONTROL_MASK)) return FALSE;
    if (mods & GDK_SHIFT_MASK) return FALSE;

    if ((keyval == GDK_KEY_z || keyval == GDK_KEY_Z) && d->on_undo) {
        d->on_undo(NULL, d->user_data);
        return TRUE;
    }
    if ((keyval == GDK_KEY_r || keyval == GDK_KEY_R) && d->on_reset) {
        d->on_reset(NULL, d->user_data);
        return TRUE;
    }
    return FALSE;
}

void image_install_edit_shortcuts(GtkWidget         *root,
                                   ImageToolCallback  on_undo,
                                   ImageToolCallback  on_reset,
                                   gpointer           user_data) {
    if (!root) return;
    ShortcutData *d = g_new0(ShortcutData, 1);
    d->on_undo = on_undo;
    d->on_reset = on_reset;
    d->user_data = user_data;

    GtkEventController *ctrl = gtk_event_controller_key_new();
    g_signal_connect(ctrl, "key-pressed", G_CALLBACK(on_shortcut_key), d);
    gtk_widget_add_controller(root, ctrl);
    g_object_set_data_full(G_OBJECT(ctrl), "shortcut-data", d, g_free);
}

/* ------------------------------------------------------------------ */
/* Undo stack                                                         */
/* ------------------------------------------------------------------ */

GPtrArray *image_undo_stack_new(void) {
    return g_ptr_array_new_with_free_func(g_object_unref);
}

void image_undo_push(GPtrArray *stack, GdkPixbuf *pixbuf) {
    if (!stack || !pixbuf) return;
    g_ptr_array_add(stack, g_object_ref(pixbuf));
}

GdkPixbuf *image_undo_pop(GPtrArray *stack) {
    if (!stack || stack->len == 0) return NULL;

    /* Take the last item out without unreffing it */
    GdkPixbuf *pb = g_ptr_array_index(stack, stack->len - 1);
    g_ptr_array_remove_index(stack, stack->len - 1);
    return pb;   /* caller owns this ref */
}

guint image_undo_depth(GPtrArray *stack) {
    return stack ? stack->len : 0;
}

void image_undo_clear(GPtrArray *stack) {
    if (stack) g_ptr_array_set_size(stack, 0);
}

void image_undo_free(GPtrArray *stack) {
    if (stack) g_ptr_array_unref(stack);
}

/* ------------------------------------------------------------------ */
/* Undo / Reset buttons                                               */
/* ------------------------------------------------------------------ */

GtkWidget *image_undo_button(GCallback on_undo, gpointer user_data) {
    GtkWidget *btn = gtk_button_new_from_icon_name("edit-undo-symbolic");
    gtk_widget_add_css_class(btn, "flat");
    gtk_widget_set_tooltip_text(btn, "Undo (Ctrl+Z)");
    if (on_undo)
        g_signal_connect(btn, "clicked", on_undo, user_data);
    return btn;
}

GtkWidget *image_reset_button(GCallback on_reset, gpointer user_data) {
    GtkWidget *btn = gtk_button_new_from_icon_name("edit-clear-symbolic");
    gtk_widget_add_css_class(btn, "flat");
    gtk_widget_set_tooltip_text(btn, "Reset to original (Ctrl+R)");
    if (on_reset)
        g_signal_connect(btn, "clicked", on_reset, user_data);
    return btn;
}
