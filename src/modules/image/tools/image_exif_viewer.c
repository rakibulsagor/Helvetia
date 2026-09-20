#include <gtk/gtk.h>
#include <adwaita.h>
#include <libexif/exif-data.h>
#include <libexif/exif-tag.h>
#include <libexif/exif-entry.h>
#include <libexif/exif-ifd.h>
#include <glib/gstdio.h>
#include "../image_shared.h"
#include "image_exif_viewer.h"

typedef struct {
    GtkWidget *stack, *list, *title, *subtitle;
    GtkWidget *file_lbl, *count_lbl, *root;
} ExifState;

static void state_free(ExifState *st) { g_free(st); }

static ExifState *get_state(GtkWidget *v) {
    return g_object_get_data(G_OBJECT(v), "exif-state");
}

/* ---- Iteration context ---- */

typedef struct {
    GtkWidget  *list;
    const char *last_group;
    guint       count;
} IterCtx;

static const char *ifd_name(ExifIfd ifd) {
    switch (ifd) {
        case EXIF_IFD_0:       return "TIFF (Image)";
        case EXIF_IFD_1:       return "TIFF (Thumbnail)";
        case EXIF_IFD_EXIF:    return "EXIF (Camera)";
        case EXIF_IFD_GPS:     return "GPS (Location)";
        case EXIF_IFD_INTEROPERABILITY: return "Interoperability";
        default:               return "Other";
    }
}

static GtkWidget *make_header(const char *text) {
    GtkWidget *row = gtk_list_box_row_new();
    gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), FALSE);
    gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(row), FALSE);
    gtk_widget_add_css_class(row, "exif-group-header");

    GtkWidget *lbl = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
    gtk_widget_set_margin_start(lbl, 12);
    gtk_widget_set_margin_top(lbl, 12);
    gtk_widget_set_margin_bottom(lbl, 4);
    gtk_widget_add_css_class(lbl, "heading");
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), lbl);
    return row;
}

static GtkWidget *make_row(const char *name, const char *value) {
    GtkWidget *row = gtk_list_box_row_new();
    gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), FALSE);
    gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(row), FALSE);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_start(box, 12);
    gtk_widget_set_margin_end(box, 12);
    gtk_widget_set_margin_top(box, 4);
    gtk_widget_set_margin_bottom(box, 4);

    GtkWidget *n = gtk_label_new(name);
    gtk_label_set_xalign(GTK_LABEL(n), 0.0f);
    gtk_label_set_width_chars(GTK_LABEL(n), 28);
    gtk_label_set_ellipsize(GTK_LABEL(n), PANGO_ELLIPSIZE_END);
    gtk_widget_add_css_class(n, "dim-label");

    GtkWidget *v = gtk_label_new(value);
    gtk_label_set_xalign(GTK_LABEL(v), 0.0f);
    gtk_widget_set_hexpand(v, TRUE);
    gtk_label_set_selectable(GTK_LABEL(v), TRUE);
    gtk_label_set_ellipsize(GTK_LABEL(v), PANGO_ELLIPSIZE_END);

    gtk_box_append(GTK_BOX(box), n);
    gtk_box_append(GTK_BOX(box), v);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
    return row;
}

static void entry_cb(ExifEntry *entry, void *user) {
    IterCtx *ctx = user;

    ExifIfd ifd = exif_entry_get_ifd(entry);
    const char *group = ifd_name(ifd);

    /* Group header if the IFD changed */
    if (g_strcmp0(group, ctx->last_group) != 0) {
        gtk_list_box_append(GTK_LIST_BOX(ctx->list), make_header(group));
        ctx->last_group = group;
    }

    const char *name = exif_tag_get_name_in_ifd(entry->tag, ifd);
    if (!name) name = "Unknown";

    char value[1024] = {0};
    exif_entry_get_value(entry, value, sizeof value);

    /* Skip empty values */
    if (value[0] == '\0') return;

    gtk_list_box_append(GTK_LIST_BOX(ctx->list), make_row(name, value));
    ctx->count++;
}

static void content_cb(ExifContent *content, void *user) {
    exif_content_foreach_entry(content, entry_cb, user);
}

/* ---- Load and render ---- */

static void clear_list(GtkWidget *list) {
    GtkWidget *c;
    while ((c = gtk_widget_get_first_child(list)))
        gtk_list_box_remove(GTK_LIST_BOX(list), c);
}

static void render_exif(GtkWidget *view, const char *path) {
    ExifState *st = get_state(view);
    clear_list(st->list);

    char *base = g_path_get_basename(path);
    gtk_label_set_text(GTK_LABEL(st->title), base);
    g_free(base);

    GStatBuf sb;
    if (g_stat(path, &sb) == 0) {
        char *sz = image_format_size(sb.st_size);
        gtk_label_set_text(GTK_LABEL(st->file_lbl), sz);
        g_free(sz);
    }

    ExifData *ed = exif_data_new_from_file(path);
    if (!ed) {
        gtk_label_set_text(GTK_LABEL(st->subtitle), "No EXIF data");
        gtk_label_set_text(GTK_LABEL(st->count_lbl), "0 fields");
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *lbl = gtk_label_new(
            "This file contains no EXIF metadata.");
        gtk_widget_set_margin_top(lbl, 32);
        gtk_widget_set_margin_bottom(lbl, 32);
        gtk_widget_add_css_class(lbl, "dim-label");
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), lbl);
        gtk_list_box_append(GTK_LIST_BOX(st->list), row);
        gtk_stack_set_visible_child_name(GTK_STACK(st->stack), "viewer");
        return;
    }

    IterCtx ctx = { .list = st->list, .last_group = NULL, .count = 0 };
    exif_data_foreach_content(ed, content_cb, &ctx);
    exif_data_unref(ed);

    if (ctx.count == 0) {
        gtk_label_set_text(GTK_LABEL(st->subtitle), "No fields found");
        gtk_label_set_text(GTK_LABEL(st->count_lbl), "0 fields");
    } else {
        gtk_label_set_text(GTK_LABEL(st->subtitle), "EXIF metadata");
        char *m = g_strdup_printf("%u fields", ctx.count);
        gtk_label_set_text(GTK_LABEL(st->count_lbl), m);
        g_free(m);
    }

    gtk_stack_set_visible_child_name(GTK_STACK(st->stack), "viewer");
}

/* ---- Callbacks ---- */

static void on_drop(const char *path, gpointer d) {
    render_exif(d, path);
}

static void on_back(GtkButton *b, gpointer d) {
    (void)b;
    ExifState *st = get_state(d);
    gtk_stack_set_visible_child_name(GTK_STACK(st->stack), "drop");
}

/* ---- Lifecycle ---- */

void image_exif_viewer_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "exif-state", NULL);
}

/* ---- Commands ---- */

typedef struct { GtkWidget *view; } OC;
static void on_open_dialog_finished(GObject *src, GAsyncResult *res, gpointer data) {
    OC *c = data;
    GError *e = NULL;
    GFile *f = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(src), res, &e);
    if (e) { g_error_free(e); g_free(c); return; }
    char *path = g_file_get_path(f);
    g_object_unref(f);
    if (path) render_exif(c->view, path);
    g_free(path);
    g_free(c);
}

static void cmd_open(GtkWidget *v) {
    ExifState *st = get_state(v);
    GtkRoot *root = gtk_widget_get_root(v);
    if (!root || !GTK_IS_WINDOW(root)) return;

    GtkFileDialog *dlg = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dlg, "Open image");
    GListStore *fs = image_filter_store_full();
    gtk_file_dialog_set_filters(dlg, G_LIST_MODEL(fs));


    OC *o = g_new0(OC, 1); o->view = v;

    gtk_file_dialog_open(dlg, GTK_WINDOW(root), NULL,
on_open_dialog_finished, o);

    g_object_unref(dlg);
    g_object_unref(fs);
    (void)st;
}

const HelvetiaToolCommand image_exif_viewer_commands[] = {
    { .id = "open", .name = "Open…",
      .icon_name = "document-open-symbolic",
      .accel = "<Control>o", .tooltip = "Open an image",
      .activate = cmd_open },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

/* ---- Create ---- */

GtkWidget *image_exif_viewer_create(void) {
    ExifState *st = g_new0(ExifState, 1);

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    gtk_widget_set_hexpand(root, TRUE);
    st->root = root;

    GtkWidget *stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(stack),
                                  GTK_STACK_TRANSITION_TYPE_CROSSFADE);
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
        "Image file (JPEG, TIFF)", on_drop, root);
    gtk_box_append(GTK_BOX(drop_box), drop);
    gtk_stack_add_named(GTK_STACK(stack), drop_box, "drop");

    /* Viewer page */
    GtkWidget *viewer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    GtkWidget *back_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_margin_start(back_box, 8);
    gtk_widget_set_margin_top(back_box, 8);
    GtkWidget *back = gtk_button_new_from_icon_name("go-previous-symbolic");
    gtk_widget_add_css_class(back, "flat");
    gtk_box_append(GTK_BOX(back_box), back);

    GtkWidget *title = gtk_label_new("");
    gtk_widget_add_css_class(title, "title-3");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0f);

    GtkWidget *subtitle = gtk_label_new("");
    gtk_widget_add_css_class(subtitle, "dim-label");
    gtk_label_set_xalign(GTK_LABEL(subtitle), 0.0f);

    GtkWidget *file_lbl = gtk_label_new("");
    gtk_widget_add_css_class(file_lbl, "dim-label");
    gtk_widget_add_css_class(file_lbl, "caption");

    GtkWidget *count_lbl = gtk_label_new("");
    gtk_widget_add_css_class(count_lbl, "dim-label");
    gtk_widget_add_css_class(count_lbl, "caption");

    st->title = title;
    st->subtitle = subtitle;
    st->file_lbl = file_lbl;
    st->count_lbl = count_lbl;

    GtkWidget *title_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_hexpand(title_box, TRUE);
    gtk_box_append(GTK_BOX(title_box), title);
    gtk_box_append(GTK_BOX(title_box), subtitle);

    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_start(header, 16);
    gtk_widget_set_margin_end(header, 16);
    gtk_widget_set_margin_top(header, 12);
    gtk_widget_set_margin_bottom(header, 12);
    gtk_box_append(GTK_BOX(header), title_box);
    gtk_box_append(GTK_BOX(header), file_lbl);
    gtk_box_append(GTK_BOX(header), count_lbl);

    GtkWidget *list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(list), GTK_SELECTION_NONE);
    gtk_widget_add_css_class(list, "exif-list");
    gtk_widget_set_margin_start(list, 8);
    gtk_widget_set_margin_end(list, 8);
    gtk_widget_set_margin_bottom(list, 16);
    st->list = list;

    GtkWidget *scroller = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), list);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scroller, TRUE);

    gtk_box_append(GTK_BOX(viewer), back_box);
    gtk_box_append(GTK_BOX(viewer), header);
    gtk_box_append(GTK_BOX(viewer), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(viewer), scroller);
    gtk_stack_add_named(GTK_STACK(stack), viewer, "viewer");

    gtk_stack_set_visible_child_name(GTK_STACK(stack), "drop");
    gtk_box_append(GTK_BOX(root), stack);

    g_object_set_data_full(G_OBJECT(root), "exif-state", st,
                           (GDestroyNotify)state_free);

    g_signal_connect(back, "clicked", G_CALLBACK(on_back), root);

    return root;
}
