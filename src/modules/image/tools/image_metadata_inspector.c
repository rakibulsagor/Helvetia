#define _GNU_SOURCE
#define _DEFAULT_SOURCE
#include <gtk/gtk.h>
#include <adwaita.h>
#include <libexif/exif-data.h>
#include <libexif/exif-tag.h>
#include <libexif/exif-entry.h>
#include <libexif/exif-ifd.h>
#include <glib/gstdio.h>
#include <sys/stat.h>
#include <string.h>

#include "../image_shared.h"
#include "image_metadata_inspector.h"

/* ------------------------------------------------------------------ */
/* State                                                              */
/* ------------------------------------------------------------------ */

typedef struct {
    GtkWidget *stack;
    GtkWidget *list;
    GtkWidget *title;
    GtkWidget *subtitle;
    GtkWidget *file_lbl;
    GtkWidget *count_lbl;
    GtkWidget *root;
    guint      row_count;
} MetadataState;

static MetadataState *get_state(GtkWidget *v) {
    return g_object_get_data(G_OBJECT(v), "metadata-state");
}

/* ------------------------------------------------------------------ */
/* Row helpers                                                        */
/* ------------------------------------------------------------------ */

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

    GtkWidget *v = gtk_label_new(value && *value ? value : "—");
    gtk_label_set_xalign(GTK_LABEL(v), 0.0f);
    gtk_widget_set_hexpand(v, TRUE);
    gtk_label_set_selectable(GTK_LABEL(v), TRUE);
    gtk_label_set_ellipsize(GTK_LABEL(v), PANGO_ELLIPSIZE_END);

    gtk_box_append(GTK_BOX(box), n);
    gtk_box_append(GTK_BOX(box), v);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
    return row;
}

static void clear_list(GtkWidget *list) {
    GtkWidget *c;
    while ((c = gtk_widget_get_first_child(list)))
        gtk_list_box_remove(GTK_LIST_BOX(list), c);
}

/* ------------------------------------------------------------------ */
/* File metadata                                                      */
/* ------------------------------------------------------------------ */

static char *format_time(time_t t) {
    GDateTime *dt = g_date_time_new_from_unix_local(t);
    if (!dt) return g_strdup("—");
    char *s = g_date_time_format(dt, "%Y-%m-%d %H:%M:%S");
    g_date_time_unref(dt);
    return s;
}

static char *format_permissions(mode_t mode) {
    char perms[11] = "----------";

    if (S_ISDIR(mode))  perms[0] = 'd';
    else if (S_ISLNK(mode)) perms[0] = 'l';
    else if (S_ISCHR(mode)) perms[0] = 'c';
    else if (S_ISBLK(mode)) perms[0] = 'b';
    else if (S_ISFIFO(mode)) perms[0] = 'p';
    else if ((mode & S_IFMT) == S_IFSOCK) perms[0] = 's';
    else perms[0] = '-';

    if (mode & S_IRUSR) perms[1] = 'r';
    if (mode & S_IWUSR) perms[2] = 'w';
    if (mode & S_IXUSR) perms[3] = 'x';
    if (mode & S_IRGRP) perms[4] = 'r';
    if (mode & S_IWGRP) perms[5] = 'w';
    if (mode & S_IXGRP) perms[6] = 'x';
    if (mode & S_IROTH) perms[7] = 'r';
    if (mode & S_IWOTH) perms[8] = 'w';
    if (mode & S_IXOTH) perms[9] = 'x';

    char *octal = g_strdup_printf("%s  (%04o)",
                                   perms, (unsigned)(mode & 07777));
    return octal;
}

static void append_file_section(MetadataState *st, const char *path) {
    GStatBuf sb;
    if (g_stat(path, &sb) != 0) return;

    gtk_list_box_append(GTK_LIST_BOX(st->list), make_header("File"));
    st->row_count++;

    char *name = g_path_get_basename(path);
    gtk_list_box_append(GTK_LIST_BOX(st->list), make_row("Name", name));
    st->row_count++;
    g_free(name);

    char *dir = g_path_get_dirname(path);
    gtk_list_box_append(GTK_LIST_BOX(st->list), make_row("Location", dir));
    st->row_count++;
    g_free(dir);

    char *size = image_format_size((guint64)sb.st_size);
    char *size_full = g_strdup_printf("%s (%" G_GINT64_FORMAT " bytes)",
                                       size, (gint64)sb.st_size);
    gtk_list_box_append(GTK_LIST_BOX(st->list),
                        make_row("Size", size_full));
    st->row_count++;
    g_free(size);
    g_free(size_full);

    char *perm = format_permissions(sb.st_mode);
    gtk_list_box_append(GTK_LIST_BOX(st->list), make_row("Permissions", perm));
    st->row_count++;
    g_free(perm);

    char *mtime = format_time(sb.st_mtime);
    gtk_list_box_append(GTK_LIST_BOX(st->list), make_row("Modified", mtime));
    st->row_count++;
    g_free(mtime);

    char *atime = format_time(sb.st_atime);
    gtk_list_box_append(GTK_LIST_BOX(st->list), make_row("Accessed", atime));
    st->row_count++;
    g_free(atime);

#if defined(__linux__)
    char *ctime = format_time(sb.st_ctime);
    gtk_list_box_append(GTK_LIST_BOX(st->list), make_row("Changed", ctime));
    st->row_count++;
    g_free(ctime);
#endif
}

/* ------------------------------------------------------------------ */
/* Image section                                                      */
/* ------------------------------------------------------------------ */

static void append_image_section(MetadataState *st, const char *path) {
    GError *e = NULL;
    GdkPixbuf *pb = gdk_pixbuf_new_from_file(path, &e);
    if (!pb) {
        if (e) g_error_free(e);
        return;
    }

    gtk_list_box_append(GTK_LIST_BOX(st->list), make_header("Image"));
    st->row_count++;

    char *dims = g_strdup_printf("%d × %d",
                                  gdk_pixbuf_get_width(pb),
                                  gdk_pixbuf_get_height(pb));
    gtk_list_box_append(GTK_LIST_BOX(st->list), make_row("Dimensions", dims));
    st->row_count++;
    g_free(dims);

    int channels = gdk_pixbuf_get_n_channels(pb);
    char *ch = g_strdup_printf("%d (%s)",
                                channels,
                                channels == 4 ? "RGBA" :
                                channels == 3 ? "RGB" :
                                channels == 2 ? "Gray+A" :
                                channels == 1 ? "Gray" : "Unknown");
    gtk_list_box_append(GTK_LIST_BOX(st->list), make_row("Channels", ch));
    st->row_count++;
    g_free(ch);

    gtk_list_box_append(GTK_LIST_BOX(st->list),
        make_row("Alpha", gdk_pixbuf_get_has_alpha(pb) ? "Yes" : "No"));
    st->row_count++;

    char *bps = g_strdup_printf("%d bits per sample",
                                 gdk_pixbuf_get_bits_per_sample(pb));
    gtk_list_box_append(GTK_LIST_BOX(st->list), make_row("Bit depth", bps));
    st->row_count++;
    g_free(bps);

    int rowstride = gdk_pixbuf_get_rowstride(pb);
    char *rs = g_strdup_printf("%d bytes", rowstride);
    gtk_list_box_append(GTK_LIST_BOX(st->list), make_row("Row stride", rs));
    st->row_count++;
    g_free(rs);

    char *ext = image_get_extension(path);
    if (ext) {
        char *up = g_ascii_strup(ext, -1);
        gtk_list_box_append(GTK_LIST_BOX(st->list), make_row("Format", up));
        st->row_count++;
        g_free(up);
        g_free(ext);
    }

    g_object_unref(pb);
}

/* ------------------------------------------------------------------ */
/* EXIF section                                                       */
/* ------------------------------------------------------------------ */

static const char *ifd_name(ExifIfd ifd) {
    switch (ifd) {
        case EXIF_IFD_0:                return "EXIF: TIFF (Image)";
        case EXIF_IFD_1:                return "EXIF: TIFF (Thumbnail)";
        case EXIF_IFD_EXIF:             return "EXIF: Camera";
        case EXIF_IFD_GPS:              return "EXIF: GPS";
        case EXIF_IFD_INTEROPERABILITY: return "EXIF: Interoperability";
        default:                        return "EXIF: Other";
    }
}

typedef struct {
    GtkWidget  *list;
    MetadataState *st;
    const char *last_group;
} IterCtx;

static void entry_cb(ExifEntry *entry, void *user) {
    IterCtx *ctx = user;
    ExifIfd ifd = exif_entry_get_ifd(entry);
    const char *group = ifd_name(ifd);

    if (g_strcmp0(group, ctx->last_group) != 0) {
        gtk_list_box_append(GTK_LIST_BOX(ctx->list), make_header(group));
        ctx->st->row_count++;
        ctx->last_group = group;
    }

    const char *name = exif_tag_get_name_in_ifd(entry->tag, ifd);
    if (!name) name = "Unknown";

    char value[1024] = {0};
    exif_entry_get_value(entry, value, sizeof value);
    if (value[0] == '\0') return;

    gtk_list_box_append(GTK_LIST_BOX(ctx->list), make_row(name, value));
    ctx->st->row_count++;
}

static void content_cb(ExifContent *content, void *user) {
    exif_content_foreach_entry(content, entry_cb, user);
}

static void append_exif_section(MetadataState *st, const char *path) {
    ExifData *ed = exif_data_new_from_file(path);
    if (!ed) return;

    IterCtx ctx = { .list = st->list, .st = st, .last_group = NULL };
    exif_data_foreach_content(ed, content_cb, &ctx);
    exif_data_unref(ed);
}

/* ------------------------------------------------------------------ */
/* Render                                                             */
/* ------------------------------------------------------------------ */

static void render_metadata(GtkWidget *view, const char *path) {
    MetadataState *st = get_state(view);
    clear_list(st->list);
    st->row_count = 0;

    /* Header */
    char *base = g_path_get_basename(path);
    gtk_label_set_text(GTK_LABEL(st->title), base);
    g_free(base);

    GStatBuf sb;
    if (g_stat(path, &sb) == 0) {
        char *sz = image_format_size((guint64)sb.st_size);
        gtk_label_set_text(GTK_LABEL(st->file_lbl), sz);
        g_free(sz);
    }

    /* Sections */
    append_file_section(st, path);
    append_image_section(st, path);
    append_exif_section(st, path);

    /* Summary */
    if (st->row_count == 0) {
        gtk_label_set_text(GTK_LABEL(st->subtitle), "No metadata found");
        gtk_label_set_text(GTK_LABEL(st->count_lbl), "");
    } else {
        gtk_label_set_text(GTK_LABEL(st->subtitle), "All metadata");
        char *m = g_strdup_printf("%u fields", st->row_count);
        gtk_label_set_text(GTK_LABEL(st->count_lbl), m);
        g_free(m);
    }

    gtk_stack_set_visible_child_name(GTK_STACK(st->stack), "viewer");
}

/* ------------------------------------------------------------------ */
/* Callbacks                                                          */
/* ------------------------------------------------------------------ */

static void on_drop(const char *path, gpointer d) {
    render_metadata(d, path);
}

static void on_back(GtkButton *b, gpointer d) {
    (void)b;
    MetadataState *st = get_state(d);
    gtk_stack_set_visible_child_name(GTK_STACK(st->stack), "drop");
}

/* ------------------------------------------------------------------ */
/* Lifecycle — single free via destroy notify                         */
/* ------------------------------------------------------------------ */

void image_metadata_inspector_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "metadata-state", NULL);
}

/* ------------------------------------------------------------------ */
/* Commands                                                           */
/* ------------------------------------------------------------------ */

typedef struct { GtkWidget *view; } OC;

static void on_open_dialog_finished(GObject *src, GAsyncResult *res, gpointer data) {
    OC *c = data;
    GError *e = NULL;
    GFile *f = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(src), res, &e);
    if (e) { g_error_free(e); g_free(c); return; }
    char *path = g_file_get_path(f);
    g_object_unref(f);
    if (path) render_metadata(c->view, path);
    g_free(path);
    g_free(c);
}

static void cmd_open(GtkWidget *v) {
    GtkRoot *root = gtk_widget_get_root(v);
    if (!root || !GTK_IS_WINDOW(root)) return;

    GtkFileDialog *dlg = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dlg, "Open file");

    GListStore *fs = g_list_store_new(GTK_TYPE_FILE_FILTER);
    g_list_store_append(fs, image_filter_images());
    g_list_store_append(fs, image_filter_all());
    gtk_file_dialog_set_filters(dlg, G_LIST_MODEL(fs));

    OC *o = g_new0(OC, 1); o->view = v;

    gtk_file_dialog_open(dlg, GTK_WINDOW(root), NULL,
        on_open_dialog_finished, o);

    g_object_unref(dlg);
    g_object_unref(fs);
}

const HelvetiaToolCommand image_metadata_inspector_commands[] = {
    { .id = "open", .name = "Open…",
      .icon_name = "document-open-symbolic",
      .accel = "<Control>o", .tooltip = "Open a file",
      .activate = cmd_open },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

/* ------------------------------------------------------------------ */
/* Create                                                             */
/* ------------------------------------------------------------------ */

GtkWidget *image_metadata_inspector_create(void) {
    MetadataState *st = g_new0(MetadataState, 1);

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
        "Any file (image, document, archive)", on_drop, root);
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
    gtk_box_append(GTK_BOX(viewer),
                   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(viewer), scroller);
    gtk_stack_add_named(GTK_STACK(stack), viewer, "viewer");

    gtk_stack_set_visible_child_name(GTK_STACK(stack), "drop");
    gtk_box_append(GTK_BOX(root), stack);

    /* Store state — free happens ONCE via this destructor */
    g_object_set_data_full(G_OBJECT(root), "metadata-state", st, g_free);

    g_signal_connect(back, "clicked", G_CALLBACK(on_back), root);

    return root;
}
