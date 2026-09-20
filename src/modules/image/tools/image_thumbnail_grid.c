#include <gtk/gtk.h>
#include <adwaita.h>
#include <cairo.h>
#include "../image_shared.h"
#include "image_thumbnail_grid.h"

#define BATCH_SIZE 20

typedef struct {
    GPtrArray *images;    /* all char* */
    GPtrArray *pending;   /* not yet in grid */
    int        thumb_size;
    guint      max_cols;
    gboolean   show_labels;
    guint      batch_id;

    GtkWidget *stack, *flow_box, *status, *full_pic, *full_lbl, *root;
    char      *folder;
} GridState;

static void state_free(GridState *st) {
    if (!st) return;
    if (st->batch_id) g_source_remove(st->batch_id);
    if (st->images) g_ptr_array_unref(st->images);
    if (st->pending) g_ptr_array_unref(st->pending);
    g_free(st->folder);
    g_free(st);
}

static GridState *get_state(GtkWidget *v) {
    return g_object_get_data(G_OBJECT(v), "thumb-grid-state");
}

static void on_thumb(GtkButton *btn, gpointer d) {
    GtkWidget *view = d;
    GridState *st = get_state(view);
    const char *path = g_object_get_data(G_OBJECT(btn), "image-path");
    if (!path) return;

    GError *e = NULL;
    GdkPixbuf *pb = gdk_pixbuf_new_from_file(path, &e);
    if (!pb) { image_show_error(view, e->message); g_error_free(e); return; }
    GdkTexture *t = gdk_texture_new_for_pixbuf(pb);
    gtk_picture_set_paintable(GTK_PICTURE(st->full_pic), GDK_PAINTABLE(t));
    gtk_picture_set_content_fit(GTK_PICTURE(st->full_pic), GTK_CONTENT_FIT_CONTAIN);
    g_object_unref(t);
    g_object_unref(pb);

    char *name = g_path_get_basename(path);
    gtk_label_set_text(GTK_LABEL(st->full_lbl), name);
    g_free(name);

    gtk_stack_set_visible_child_name(GTK_STACK(st->stack), "full");
}

static GtkWidget *build_thumb(GtkWidget *view, const char *path,
                               int size, gboolean show_labels) {
    GtkWidget *btn = gtk_button_new();
    gtk_widget_add_css_class(btn, "thumbnail-cell");
    gtk_widget_set_tooltip_text(btn, path);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);

    GtkWidget *pic = gtk_picture_new_for_filename(path);
    gtk_picture_set_can_shrink(GTK_PICTURE(pic), TRUE);
    gtk_picture_set_content_fit(GTK_PICTURE(pic), GTK_CONTENT_FIT_COVER);
    gtk_widget_set_size_request(pic, size, size);
    gtk_widget_add_css_class(pic, "thumbnail-picture");
    gtk_box_append(GTK_BOX(vbox), pic);

    if (show_labels) {
        char *nm = g_path_get_basename(path);
        GtkWidget *lbl = gtk_label_new(nm);
        gtk_label_set_ellipsize(GTK_LABEL(lbl), PANGO_ELLIPSIZE_MIDDLE);
        gtk_label_set_max_width_chars(GTK_LABEL(lbl), 20);
        gtk_widget_add_css_class(lbl, "caption");
        gtk_widget_add_css_class(lbl, "dim-label");
        gtk_box_append(GTK_BOX(vbox), lbl);
        g_free(nm);
    }

    gtk_button_set_child(GTK_BUTTON(btn), vbox);
    g_object_set_data_full(G_OBJECT(btn), "image-path", g_strdup(path), g_free);
    g_signal_connect(btn, "clicked", G_CALLBACK(on_thumb), view);
    return btn;
}

static gboolean add_batch(gpointer d) {
    GtkWidget *view = d;
    GridState *st = get_state(view);

    if (!st->pending || st->pending->len == 0) {
        char *m = g_strdup_printf("%u images", st->images->len);
        gtk_label_set_text(GTK_LABEL(st->status), m);
        g_free(m);
        st->batch_id = 0;
        return G_SOURCE_REMOVE;
    }

    guint take = MIN(BATCH_SIZE, st->pending->len);
    for (guint i = 0; i < take; i++) {
        GtkWidget *t = build_thumb(view, g_ptr_array_index(st->pending, i),
                                    st->thumb_size, st->show_labels);
        gtk_flow_box_append(GTK_FLOW_BOX(st->flow_box), t);
    }
    g_ptr_array_remove_range(st->pending, 0, take);

    guint loaded = st->images->len - st->pending->len;
    char *m = g_strdup_printf("Loading… %u / %u", loaded, st->images->len);
    gtk_label_set_text(GTK_LABEL(st->status), m);
    g_free(m);
    return G_SOURCE_CONTINUE;
}

static void schedule_batch(GtkWidget *view) {
    GridState *st = get_state(view);
    if (st->batch_id) g_source_remove(st->batch_id);
    st->batch_id = g_idle_add(add_batch, view);
}

static void clear_flow(GtkWidget *fb) {
    GtkWidget *c;
    while ((c = gtk_widget_get_first_child(fb)))
        gtk_flow_box_remove(GTK_FLOW_BOX(fb), c);
}

static gint cmp_str(gconstpointer a, gconstpointer b) {
    return g_strcmp0(*(const char **)a, *(const char **)b);
}

static void load_folder(GtkWidget *view, const char *file_path) {
    GridState *st = get_state(view);
    if (st->batch_id) { g_source_remove(st->batch_id); st->batch_id = 0; }
    clear_flow(st->flow_box);

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

    if (st->images->len == 0) {
        image_show_error(view, "No images found in folder");
        g_free(dir_path);
        return;
    }
    g_ptr_array_sort(st->images, cmp_str);

    g_free(st->folder);
    st->folder = g_strdup(dir_path);
    g_free(dir_path);

    if (st->pending) g_ptr_array_unref(st->pending);
    st->pending = g_ptr_array_new_with_free_func(g_free);
    for (guint i = 0; i < st->images->len; i++)
        g_ptr_array_add(st->pending,
                        g_strdup(g_ptr_array_index(st->images, i)));

    char *m = g_strdup_printf("Loading… 0 / %u", st->images->len);
    gtk_label_set_text(GTK_LABEL(st->status), m);
    g_free(m);

    gtk_stack_set_visible_child_name(GTK_STACK(st->stack), "grid");
    schedule_batch(view);
}

static void set_size(GtkWidget *view, int size) {
    GridState *st = get_state(view);
    if (st->thumb_size == size) return;
    st->thumb_size = size;

    GtkWidget *c = gtk_widget_get_first_child(st->flow_box);
    while (c) {
        GtkWidget *vbox = gtk_button_get_child(GTK_BUTTON(c));
        if (vbox) {
            GtkWidget *pic = gtk_widget_get_first_child(vbox);
            if (pic && GTK_IS_PICTURE(pic))
                gtk_widget_set_size_request(pic, size, size);
        }
        c = gtk_widget_get_next_sibling(c);
    }
}

static void set_cols(GtkWidget *view, guint cols) {
    GridState *st = get_state(view);
    st->max_cols = cols;
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(st->flow_box), cols);
}

static void toggle_labels(GtkWidget *view, gboolean show) {
    GridState *st = get_state(view);
    st->show_labels = show;
    if (!st->images) return;

    clear_flow(st->flow_box);
    for (guint i = 0; i < st->images->len; i++) {
        GtkWidget *t = build_thumb(view, g_ptr_array_index(st->images, i),
                                    st->thumb_size, show);
        gtk_flow_box_append(GTK_FLOW_BOX(st->flow_box), t);
    }
}

/* ---- Contact sheet worker ---- */

typedef struct {
    char **paths;
    int    n;
    int    thumb_size;
    guint  cols;
    char  *out_path;
} SheetJob;

static void sheet_job_free(SheetJob *j) {
    if (!j) return;
    for (int i = 0; i < j->n; i++) g_free(j->paths[i]);
    g_free(j->paths);
    g_free(j->out_path);
    g_free(j);
}

static void sheet_worker(GTask *task, gpointer src, gpointer data, GCancellable *c) {
    (void)src;
    SheetJob *j = data;
    if (g_cancellable_is_cancelled(c)) {
        g_task_return_new_error(task, G_IO_ERROR, G_IO_ERROR_CANCELLED, "Cancelled");
        return;
    }

    guint cols = j->cols > 0 ? j->cols : 1;
    guint rows = (j->n + cols - 1) / cols;
    int pad = 8;
    int cell = j->thumb_size + pad;
    int W = (int)cols * cell + pad;
    int H = (int)rows * cell + pad;

    cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, W, H);
    cairo_t *cr = cairo_create(surf);
    cairo_set_source_rgb(cr, 0.12, 0.12, 0.12);
    cairo_paint(cr);

    for (int i = 0; i < j->n; i++) {
        GError *e = NULL;
        GdkPixbuf *pb = gdk_pixbuf_new_from_file_at_scale(
            j->paths[i], j->thumb_size, j->thumb_size, TRUE, &e);
        if (!pb) { if (e) g_error_free(e); continue; }

        int pw = gdk_pixbuf_get_width(pb);
        int ph = gdk_pixbuf_get_height(pb);
        int row = i / cols;
        int col = i % cols;
        int x = pad + col * cell + (j->thumb_size - pw) / 2;
        int y = pad + row * cell + (j->thumb_size - ph) / 2;

        gdk_cairo_set_source_pixbuf(cr, pb, x, y);
        cairo_paint(cr);
        g_object_unref(pb);
    }

    cairo_destroy(cr);
    cairo_status_t status = cairo_surface_write_to_png(surf, j->out_path);
    cairo_surface_destroy(surf);

    if (status != CAIRO_STATUS_SUCCESS) {
        g_task_return_new_error(task, G_IO_ERROR, G_IO_ERROR_FAILED,
                                "PNG write failed");
        return;
    }
    g_task_return_boolean(task, TRUE);
}

static void sheet_done(GObject *s, GAsyncResult *r, gpointer d) {
    (void)s;
    GtkWidget *view = d;
    GError *e = NULL;
    g_task_propagate_boolean(G_TASK(r), &e);
    if (e) { image_show_error(view, e->message); g_error_free(e); return; }
    image_show_info(view, "Contact sheet saved");
}

typedef struct { SheetJob *job; GtkWidget *view; } Ctx;
static void on_export_dialog_finished(GObject *src, GAsyncResult *res, gpointer data) {
    Ctx *c = data;
    GError *e = NULL;
    GFile *f = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(src), res, &e);
    if (e) { g_error_free(e); sheet_job_free(c->job); g_free(c); return; }
    c->job->out_path = g_file_get_path(f);
    g_object_unref(f);
    GTask *t = g_task_new(NULL, NULL, sheet_done, c->view);
    g_task_set_task_data(t, c->job, (GDestroyNotify)sheet_job_free);
    g_task_run_in_thread(t, sheet_worker);
    g_object_unref(t);
    g_free(c);
}

static void export_sheet(GtkWidget *view) {
    GridState *st = get_state(view);
    if (!st->images || st->images->len == 0) {
        image_show_error(view, "No images to export");
        return;
    }

    GtkFileDialog *dlg = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dlg, "Save contact sheet");
    gtk_file_dialog_set_initial_name(dlg, "contact_sheet.png");

    /* Capture paths for the dialog callback */
    SheetJob *job = g_new0(SheetJob, 1);
    job->n = st->images->len;
    job->paths = g_new0(char *, job->n);
    for (int i = 0; i < job->n; i++)
        job->paths[i] = g_strdup(g_ptr_array_index(st->images, i));
    job->thumb_size = st->thumb_size;
    job->cols = st->max_cols;

    /* Inline callback */

    Ctx *ctx = g_new0(Ctx, 1);
    ctx->job = job;
    ctx->view = view;

    gtk_file_dialog_save(dlg, GTK_WINDOW(gtk_widget_get_root(view)), NULL,
on_export_dialog_finished, ctx);

    g_object_unref(dlg);
}

/* ---- Callbacks ---- */

static void on_drop(const char *path, gpointer d) { load_folder(d, path); }

static void on_size(GObject *dd, GParamSpec *p, gpointer d) {
    (void)p;
    static const int s[] = {96, 144, 192, 256};
    guint i = gtk_drop_down_get_selected(GTK_DROP_DOWN(dd));
    if (i < G_N_ELEMENTS(s)) set_size(d, s[i]);
}

static void on_cols(GObject *dd, GParamSpec *p, gpointer d) {
    (void)p;
    static const guint c[] = {6, 2, 3, 4, 5, 6, 8};
    guint i = gtk_drop_down_get_selected(GTK_DROP_DOWN(dd));
    if (i < G_N_ELEMENTS(c)) set_cols(d, c[i]);
}

static void on_labels(GObject *sw, GParamSpec *p, gpointer d) {
    (void)p;
    toggle_labels(d, gtk_switch_get_active(GTK_SWITCH(sw)));
}

static void on_back(GtkButton *b, gpointer d) {
    (void)b;
    GridState *st = get_state(d);
    gtk_stack_set_visible_child_name(GTK_STACK(st->stack), "grid");
}

static void on_refresh(GtkButton *b, gpointer d) {
    (void)b;
    GridState *st = get_state(d);
    if (st->folder) load_folder(d, st->folder);
}

typedef struct { GtkWidget *view; } OC;
static void on_open_dialog_finished(GObject *src, GAsyncResult *res, gpointer data) {
    OC *c = data;
    GError *e = NULL;
    GFile *f = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(src), res, &e);
    if (e) { g_error_free(e); g_free(c); return; }
    char *path = g_file_get_path(f);
    g_object_unref(f);
    if (path) load_folder(c->view, path);
    g_free(path);
    g_free(c);
}

static void on_open(GtkButton *b, gpointer d) {
    GtkWidget *view = d;
    GtkRoot *root = gtk_widget_get_root(view);
    if (!root || !GTK_IS_WINDOW(root)) return;
    GtkFileDialog *dlg = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dlg, "Open images");
    GListStore *fs = image_filter_store_full();
    gtk_file_dialog_set_filters(dlg, G_LIST_MODEL(fs));


    OC *o = g_new0(OC, 1); o->view = view;

    gtk_file_dialog_open(dlg, GTK_WINDOW(root), NULL,
on_open_dialog_finished, o);

    g_object_unref(dlg);
    g_object_unref(fs);
}

static void on_export(GtkButton *b, gpointer d) { (void)b; export_sheet(d); }

/* ---- Lifecycle ---- */

void image_thumbnail_grid_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "thumb-grid-state", NULL);
}

/* ---- Commands ---- */

static void cmd_open(GtkWidget *v)    { on_open(NULL, v); }
static void cmd_refresh(GtkWidget *v) { on_refresh(NULL, v); }
static void cmd_export(GtkWidget *v)  { export_sheet(v); }

const HelvetiaToolCommand image_thumbnail_grid_commands[] = {
    { .id = "open", .name = "Open Folder…",
      .icon_name = "folder-open-symbolic",
      .accel = "<Control>o", .tooltip = "Open a folder",
      .activate = cmd_open },
    { .id = "refresh", .name = "Refresh",
      .icon_name = "view-refresh-symbolic",
      .accel = "F5", .tooltip = "Rescan folder",
      .activate = cmd_refresh },
    { .id = "export", .name = "Export Contact Sheet…",
      .icon_name = "document-save-symbolic",
      .accel = "<Control><Shift>s", .tooltip = "Save as PNG",
      .activate = cmd_export },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

/* ---- Create ---- */

GtkWidget *image_thumbnail_grid_create(void) {
    GridState *st = g_new0(GridState, 1);
    st->thumb_size = 144;
    st->max_cols = 6;

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
        "Image or folder (drag a file to open its folder)", on_drop, root);
    gtk_box_append(GTK_BOX(drop_box), drop);
    gtk_stack_add_named(GTK_STACK(stack), drop_box, "drop");

    /* Grid page */
    GtkWidget *grid_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(toolbar, 12);
    gtk_widget_set_margin_end(toolbar, 12);
    gtk_widget_set_margin_top(toolbar, 8);
    gtk_widget_set_margin_bottom(toolbar, 8);

    GtkWidget *open_btn = gtk_button_new_from_icon_name("folder-open-symbolic");
    GtkWidget *ref_btn = gtk_button_new_from_icon_name("view-refresh-symbolic");
    gtk_widget_add_css_class(open_btn, "flat");
    gtk_widget_add_css_class(ref_btn, "flat");

    GtkWidget *sz_lbl = gtk_label_new("Size");
    gtk_widget_add_css_class(sz_lbl, "dim-label");
    const char *sz_items[] = {"Small", "Medium", "Large", "Extra", NULL};
    GtkWidget *sz_dd = gtk_drop_down_new_from_strings(sz_items);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(sz_dd), 1);

    GtkWidget *cl_lbl = gtk_label_new("Columns");
    gtk_widget_add_css_class(cl_lbl, "dim-label");
    const char *cl_items[] = {"Auto", "2", "3", "4", "5", "6", "8", NULL};
    GtkWidget *cl_dd = gtk_drop_down_new_from_strings(cl_items);

    GtkWidget *lb_lbl = gtk_label_new("Labels");
    gtk_widget_add_css_class(lb_lbl, "dim-label");
    GtkWidget *lb_sw = gtk_switch_new();
    gtk_widget_set_valign(lb_sw, GTK_ALIGN_CENTER);

    GtkWidget *exp_btn = gtk_button_new_with_label("Export…");
    gtk_widget_add_css_class(exp_btn, "flat");

    gtk_box_append(GTK_BOX(toolbar), open_btn);
    gtk_box_append(GTK_BOX(toolbar), ref_btn);
    gtk_box_append(GTK_BOX(toolbar), gtk_separator_new(GTK_ORIENTATION_VERTICAL));
    gtk_box_append(GTK_BOX(toolbar), sz_lbl);
    gtk_box_append(GTK_BOX(toolbar), sz_dd);
    gtk_box_append(GTK_BOX(toolbar), cl_lbl);
    gtk_box_append(GTK_BOX(toolbar), cl_dd);
    gtk_box_append(GTK_BOX(toolbar), lb_lbl);
    gtk_box_append(GTK_BOX(toolbar), lb_sw);
    gtk_box_append(GTK_BOX(toolbar), gtk_separator_new(GTK_ORIENTATION_VERTICAL));
    gtk_box_append(GTK_BOX(toolbar), exp_btn);

    GtkWidget *flow = gtk_flow_box_new();
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flow), GTK_SELECTION_NONE);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(flow), TRUE);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(flow), 8);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(flow), 8);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(flow), 2);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flow), 6);
    gtk_widget_set_margin_start(flow, 16);
    gtk_widget_set_margin_end(flow, 16);
    gtk_widget_set_margin_top(flow, 16);
    gtk_widget_set_margin_bottom(flow, 16);
    st->flow_box = flow;

    GtkWidget *scroller = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), flow);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scroller, TRUE);

    GtkWidget *status = gtk_label_new("");
    gtk_widget_add_css_class(status, "dim-label");
    gtk_widget_add_css_class(status, "caption");
    gtk_label_set_xalign(GTK_LABEL(status), 0.0f);
    gtk_widget_set_margin_start(status, 12);
    gtk_widget_set_margin_bottom(status, 6);
    st->status = status;

    gtk_box_append(GTK_BOX(grid_page), toolbar);
    gtk_box_append(GTK_BOX(grid_page), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(grid_page), scroller);
    gtk_box_append(GTK_BOX(grid_page), status);
    gtk_stack_add_named(GTK_STACK(stack), grid_page, "grid");

    /* Full page */
    GtkWidget *full_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *fh = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(fh, 12);
    gtk_widget_set_margin_end(fh, 12);
    gtk_widget_set_margin_top(fh, 8);
    gtk_widget_set_margin_bottom(fh, 8);
    GtkWidget *back = gtk_button_new_from_icon_name("go-previous-symbolic");
    gtk_widget_add_css_class(back, "flat");
    GtkWidget *fl = gtk_label_new("");
    gtk_widget_add_css_class(fl, "title-4");
    gtk_label_set_ellipsize(GTK_LABEL(fl), PANGO_ELLIPSIZE_MIDDLE);
    gtk_widget_set_hexpand(fl, TRUE);
    st->full_lbl = fl;
    gtk_box_append(GTK_BOX(fh), back);
    gtk_box_append(GTK_BOX(fh), fl);

    GtkWidget *fp = gtk_picture_new();
    gtk_picture_set_can_shrink(GTK_PICTURE(fp), TRUE);
    gtk_picture_set_content_fit(GTK_PICTURE(fp), GTK_CONTENT_FIT_CONTAIN);
    gtk_widget_set_vexpand(fp, TRUE);
    st->full_pic = fp;

    gtk_box_append(GTK_BOX(full_page), fh);
    gtk_box_append(GTK_BOX(full_page), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(full_page), fp);
    gtk_stack_add_named(GTK_STACK(stack), full_page, "full");

    gtk_stack_set_visible_child_name(GTK_STACK(stack), "drop");
    gtk_box_append(GTK_BOX(root), stack);

    g_object_set_data_full(G_OBJECT(root), "thumb-grid-state", st,
                           (GDestroyNotify)state_free);

    g_signal_connect(open_btn, "clicked", G_CALLBACK(on_open), root);
    g_signal_connect(ref_btn, "clicked", G_CALLBACK(on_refresh), root);
    g_signal_connect(sz_dd, "notify::selected", G_CALLBACK(on_size), root);
    g_signal_connect(cl_dd, "notify::selected", G_CALLBACK(on_cols), root);
    g_signal_connect(lb_sw, "notify::active", G_CALLBACK(on_labels), root);
    g_signal_connect(exp_btn, "clicked", G_CALLBACK(on_export), root);
    g_signal_connect(back, "clicked", G_CALLBACK(on_back), root);

    return root;
}
