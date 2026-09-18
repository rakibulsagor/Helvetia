#include "qr_studio.h"
#include "qrcodegen.h"
#include "quirc/quirc.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define PI 3.14159265358979323846

/* ------------------------------------------------------------------------- */
/* State                                                                     */
/* ------------------------------------------------------------------------- */
typedef struct {
    GtkWidget *text_view;
    GtkWidget *ecc_combo;
    GtkWidget *size_combo;
    GtkWidget *preview_area;
    
    GtkWidget *btn_square;
    GtkWidget *btn_round;
    GtkWidget *btn_dots;
    GtkWidget *btn_minimal;
    
    uint8_t qrcode[qrcodegen_BUFFER_LEN_MAX];
    gboolean has_qr;
    int style; // 0: Square, 1: Round, 2: Dots, 3: Minimal
    
    GdkTexture *logo_tex;
    
    // Scanner
    GtkWidget *scan_drop;
    GtkWidget *scan_result;
} QrState;

/* ------------------------------------------------------------------------- */
/* Helpers                                                                   */
/* ------------------------------------------------------------------------- */

static void do_generate(QrState *state) {
    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->text_view));
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buf, &start, &end);
    char *text = gtk_text_buffer_get_text(buf, &start, &end, FALSE);
    
    if (!text || strlen(text) == 0) {
        state->has_qr = FALSE;
        gtk_widget_queue_draw(state->preview_area);
        g_free(text);
        return;
    }
    
    enum qrcodegen_Ecc ecc = qrcodegen_Ecc_MEDIUM;
    int ecc_sel = gtk_drop_down_get_selected(GTK_DROP_DOWN(state->ecc_combo));
    if (ecc_sel == 0) ecc = qrcodegen_Ecc_LOW;
    if (ecc_sel == 1) ecc = qrcodegen_Ecc_MEDIUM;
    if (ecc_sel == 2) ecc = qrcodegen_Ecc_QUARTILE;
    if (ecc_sel == 3) ecc = qrcodegen_Ecc_HIGH;
    
    uint8_t temp[qrcodegen_BUFFER_LEN_MAX];
    bool ok = qrcodegen_encodeText(text, temp, state->qrcode, ecc, 
                                   qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX, 
                                   qrcodegen_Mask_AUTO, true);
    state->has_qr = ok;
    g_free(text);
    gtk_widget_queue_draw(state->preview_area);
}

static void on_generate_clicked(GtkButton *btn, gpointer data) {
    do_generate(data);
}

static void on_style_toggled(GtkToggleButton *btn, gpointer data) {
    QrState *state = data;
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(state->btn_square))) state->style = 0;
    else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(state->btn_round))) state->style = 1;
    else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(state->btn_dots))) state->style = 2;
    else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(state->btn_minimal))) state->style = 3;
    
    gtk_widget_queue_draw(state->preview_area);
}

static void draw_qr_cairo(cairo_t *cr, QrState *state, int w, int h) {
    // White background
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);
    
    if (!state->has_qr) return;
    
    int size = qrcodegen_getSize(state->qrcode);
    int border = 4;
    if (state->style == 3) border = 2; // Minimal
    
    int box_size = size + border * 2;
    double scale = (double)(w < h ? w : h) / box_size;
    
    cairo_translate(cr, (w - box_size * scale)/2.0, (h - box_size * scale)/2.0);
    cairo_scale(cr, scale, scale);
    
    cairo_set_source_rgb(cr, 0, 0, 0); // Black dots
    
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            if (qrcodegen_getModule(state->qrcode, x, y)) {
                double px = x + border;
                double py = y + border;
                
                if (state->style == 0 || state->style == 3) {
                    // Square
                    cairo_rectangle(cr, px, py, 1.01, 1.01);
                    cairo_fill(cr);
                } else if (state->style == 1) {
                    // Round (slight radius)
                    // Quick approximation using arcs
                    cairo_arc(cr, px + 0.5, py + 0.5, 0.5, 0, 2*PI);
                    cairo_fill(cr);
                } else if (state->style == 2) {
                    // Dots (smaller circles)
                    cairo_arc(cr, px + 0.5, py + 0.5, 0.35, 0, 2*PI);
                    cairo_fill(cr);
                }
            }
        }
    }
    
    // Draw Logo in center if present
    if (state->logo_tex) {
        int lw = gdk_texture_get_width(state->logo_tex);
        int lh = gdk_texture_get_height(state->logo_tex);
        // We want logo to take up at most ~20% of QR size
        double target_size = size * 0.25;
        double s = target_size / (lw > lh ? lw : lh);
        
        double lx = border + (size - lw * s) / 2.0;
        double ly = border + (size - lh * s) / 2.0;
        
        // White backdrop for logo
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_rectangle(cr, lx - 1, ly - 1, lw*s + 2, lh*s + 2);
        cairo_fill(cr);
        
        int stride = lw * 4;
        guchar *pixels = g_malloc(lh * stride);
        gdk_texture_download(state->logo_tex, pixels, stride);
        cairo_surface_t *surf = cairo_image_surface_create_for_data(pixels, CAIRO_FORMAT_ARGB32, lw, lh, stride);
        
        cairo_save(cr);
        cairo_translate(cr, lx, ly);
        cairo_scale(cr, s, s);
        cairo_set_source_surface(cr, surf, 0, 0);
        cairo_paint(cr);
        cairo_restore(cr);
        
        cairo_surface_destroy(surf);
        g_free(pixels);
    }
}

static void on_preview_draw(GtkDrawingArea *area, cairo_t *cr, int w, int h, gpointer data) {
    draw_qr_cairo(cr, data, w, h);
}

static void save_png_surface(QrState *state, const char *path) {
    if (!state->has_qr) return;
    int sz = 1024;
    cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, sz, sz);
    cairo_t *cr = cairo_create(surf);
    draw_qr_cairo(cr, state, sz, sz);
    cairo_surface_write_to_png(surf, path);
    cairo_destroy(cr);
    cairo_surface_destroy(surf);
}

static void on_logo_open_response(GObject *source_object, GAsyncResult *res, gpointer data) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);
    GError *error = NULL;
    GFile *file = gtk_file_dialog_open_finish(dialog, res, &error);
    if (file) {
        QrState *state = data;
        if (state->logo_tex) g_object_unref(state->logo_tex);
        state->logo_tex = gdk_texture_new_from_file(file, NULL);
        gtk_widget_queue_draw(state->preview_area);
        g_object_unref(file);
    }
}

static void on_upload_logo(GtkButton *btn, gpointer data) {
    QrState *state = data;
    GtkFileDialog *dialog = gtk_file_dialog_new();
    GtkWidget *win = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(btn)));
    gtk_file_dialog_open(dialog, GTK_WINDOW(win), NULL, on_logo_open_response, state);
    g_object_unref(dialog);
}

static void on_save_response(GObject *source_object, GAsyncResult *res, gpointer data) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);
    GError *error = NULL;
    GFile *file = gtk_file_dialog_save_finish(dialog, res, &error);
    if (file) {
        char *path = g_file_get_path(file);
        if (path) {
            QrState *state = data;
            save_png_surface(state, path);
            g_free(path);
        }
        g_object_unref(file);
    }
}


static void process_scan_texture(QrState *state, GdkTexture *tex) {
    if (!tex) return;
    int w = gdk_texture_get_width(tex);
    int h = gdk_texture_get_height(tex);
    if (w == 0 || h == 0) return;
    
    int stride = w * 4;
    uint8_t *pixels = g_malloc(h * stride);
    gdk_texture_download(tex, pixels, stride);
    
    struct quirc *qr = quirc_new();
    if (!qr) { g_free(pixels); return; }
    if (quirc_resize(qr, w, h) < 0) { quirc_destroy(qr); g_free(pixels); return; }
    
    int qw, qh;
    uint8_t *gray = quirc_begin(qr, &qw, &qh);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int p = y * stride + x * 4;
            uint8_t b = pixels[p];
            uint8_t g = pixels[p+1];
            uint8_t r = pixels[p+2];
            gray[y * w + x] = (uint8_t)(r*0.299 + g*0.587 + b*0.114);
        }
    }
    quirc_end(qr);
    g_free(pixels);
    
    int count = quirc_count(qr);
    if (count == 0) {
        gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->scan_result)), "No QR code found in the image.", -1);
    } else {
        struct quirc_code code;
        struct quirc_data data;
        quirc_extract(qr, 0, &code);
        quirc_decode_error_t err = quirc_decode(&code, &data);
        if (err) {
            gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->scan_result)), quirc_strerror(err), -1);
        } else {
            gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->scan_result)), (const char *)data.payload, data.payload_len);
        }
    }
    quirc_destroy(qr);
}

static void on_scan_open_response(GObject *source_object, GAsyncResult *res, gpointer data) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);
    GError *error = NULL;
    GFile *file = gtk_file_dialog_open_finish(dialog, res, &error);
    if (file) {
        QrState *state = data;
        GdkTexture *tex = gdk_texture_new_from_file(file, NULL);
        if (tex) {
            process_scan_texture(state, tex);
            g_object_unref(tex);
        }
        g_object_unref(file);
    }
}

static void on_scan_upload(GtkButton *btn, gpointer data) {
    QrState *state = data;
    GtkFileDialog *dialog = gtk_file_dialog_new();
    GtkWidget *win = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(btn)));
    gtk_file_dialog_open(dialog, GTK_WINDOW(win), NULL, on_scan_open_response, state);
    g_object_unref(dialog);
}

static gboolean on_scan_drop(GtkDropTarget *target, const GValue *value, double x, double y, gpointer data) {
    if (G_VALUE_HOLDS(value, GDK_TYPE_FILE_LIST)) {
        GdkFileList *file_list = g_value_get_boxed(value);
        GSList *files = gdk_file_list_get_files(file_list);
        if (files) {
            GFile *file = files->data;
            QrState *state = data;
            GdkTexture *tex = gdk_texture_new_from_file(file, NULL);
            if (tex) {
                process_scan_texture(state, tex);
                g_object_unref(tex);
            }
            return TRUE;
        }
    }
    return FALSE;
}

static gboolean on_logo_drop(GtkDropTarget *target, const GValue *value, double x, double y, gpointer data) {
    if (G_VALUE_HOLDS(value, GDK_TYPE_FILE_LIST)) {
        GdkFileList *file_list = g_value_get_boxed(value);
        GSList *files = gdk_file_list_get_files(file_list);
        if (files) {
            GFile *file = files->data;
            QrState *state = data;
            if (state->logo_tex) g_object_unref(state->logo_tex);
            state->logo_tex = gdk_texture_new_from_file(file, NULL);
            gtk_widget_queue_draw(state->preview_area);
            return TRUE;
        }
    }
    return FALSE;
}

static void on_download_png(GtkButton *btn, gpointer data) {
    QrState *state = data;
    if (!state->has_qr) return;
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_initial_name(dialog, "qrcode.png");
    GtkWidget *win = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(btn)));
    gtk_file_dialog_save(dialog, GTK_WINDOW(win), NULL, on_save_response, state);
    g_object_unref(dialog);
}

/* ------------------------------------------------------------------------- */
/* Builder                                                                   */
/* ------------------------------------------------------------------------- */

static GtkWidget *make_style_btn(const char *label, const char *sub, GtkWidget *group) {
    GtkWidget *btn = gtk_toggle_button_new();
    if (group) gtk_toggle_button_set_group(GTK_TOGGLE_BUTTON(btn), GTK_TOGGLE_BUTTON(group));
    
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_margin_top(box, 8); gtk_widget_set_margin_bottom(box, 8);
    GtkWidget *l1 = gtk_label_new(label);
    gtk_widget_add_css_class(l1, "heading");
    GtkWidget *l2 = gtk_label_new(sub);
    gtk_widget_add_css_class(l2, "dim-label");
    
    gtk_box_append(GTK_BOX(box), l1);
    gtk_box_append(GTK_BOX(box), l2);
    gtk_button_set_child(GTK_BUTTON(btn), box);
    gtk_widget_set_hexpand(btn, TRUE);
    
    return btn;
}

GtkWidget *build_qr_studio(void) {
    QrState *state = g_new0(QrState, 1);
    
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    
    GtkWidget *main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 24);
    gtk_widget_set_margin_top(main_vbox, 20);
    gtk_widget_set_margin_bottom(main_vbox, 20);
    gtk_widget_set_margin_start(main_vbox, 20);
    gtk_widget_set_margin_end(main_vbox, 20);
    g_signal_connect_swapped(main_vbox, "destroy", G_CALLBACK(g_free), state);
    
    // Header
    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *title = gtk_label_new("Professional QR Code Generator & Scanner");
    gtk_widget_add_css_class(title, "title-1");
    GtkWidget *subtitle = gtk_label_new("Create and scan QR codes instantly with custom styles, logos, and multiple formats");
    gtk_widget_add_css_class(subtitle, "dim-label");
    gtk_box_append(GTK_BOX(header_box), title);
    gtk_box_append(GTK_BOX(header_box), subtitle);
    gtk_box_append(GTK_BOX(main_vbox), header_box);
    
    // ==========================================
    // GENERATOR CARD
    // ==========================================
    GtkWidget *gen_frame = gtk_frame_new(NULL);
    GtkWidget *gen_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 24);
    gtk_widget_set_margin_top(gen_box, 16); gtk_widget_set_margin_bottom(gen_box, 16);
    gtk_widget_set_margin_start(gen_box, 16); gtk_widget_set_margin_end(gen_box, 16);
    gtk_frame_set_child(GTK_FRAME(gen_frame), gen_box);
    
    // Left: Gen Options
    GtkWidget *gen_left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_hexpand(gen_left, TRUE);
    
    GtkWidget *txt_lbl = gtk_label_new("Enter text, URL, or data:");
    gtk_widget_set_halign(txt_lbl, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(gen_left), txt_lbl);
    
    state->text_view = gtk_text_view_new();
    gtk_widget_set_size_request(state->text_view, -1, 80);
    gtk_widget_add_css_class(state->text_view, "view");
    GtkWidget *txt_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(txt_scroll), state->text_view);
    gtk_widget_add_css_class(txt_scroll, "frame");
    gtk_box_append(GTK_BOX(gen_left), txt_scroll);
    
    GtkWidget *row2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *ecc_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_box_append(GTK_BOX(ecc_box), gtk_label_new("Error correction level:"));
    const char *eccs[] = {"Low (7%)", "Medium (15%)", "Quartile (25%)", "High (30%)", NULL};
    state->ecc_combo = gtk_drop_down_new_from_strings(eccs);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(state->ecc_combo), 1);
    gtk_box_append(GTK_BOX(ecc_box), state->ecc_combo);
    gtk_widget_set_hexpand(ecc_box, TRUE);
    
    GtkWidget *size_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_box_append(GTK_BOX(size_box), gtk_label_new("Size (pixels):"));
    const char *sizes[] = {"300x300", "500x500", "1024x1024", NULL};
    state->size_combo = gtk_drop_down_new_from_strings(sizes);
    gtk_box_append(GTK_BOX(size_box), state->size_combo);
    gtk_widget_set_hexpand(size_box, TRUE);
    
    gtk_box_append(GTK_BOX(row2), ecc_box);
    gtk_box_append(GTK_BOX(row2), size_box);
    gtk_box_append(GTK_BOX(gen_left), row2);
    
    gtk_box_append(GTK_BOX(gen_left), gtk_label_new("QR Code Style:"));
    GtkWidget *style_grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(style_grid), 8);
    gtk_grid_set_row_spacing(GTK_GRID(style_grid), 8);
    
    state->btn_square = make_style_btn("■ Square", "Classic blocks", NULL);
    state->btn_round = make_style_btn("● Round", "Circular dots", state->btn_square);
    state->btn_dots = make_style_btn("• Dots", "Small circles", state->btn_square);
    state->btn_minimal = make_style_btn("□ Minimal", "Compact style", state->btn_square);
    
    gtk_grid_attach(GTK_GRID(style_grid), state->btn_square, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(style_grid), state->btn_round, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(style_grid), state->btn_dots, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(style_grid), state->btn_minimal, 1, 1, 1, 1);
    gtk_box_append(GTK_BOX(gen_left), style_grid);
    
    g_signal_connect(state->btn_square, "toggled", G_CALLBACK(on_style_toggled), state);
    g_signal_connect(state->btn_round, "toggled", G_CALLBACK(on_style_toggled), state);
    g_signal_connect(state->btn_dots, "toggled", G_CALLBACK(on_style_toggled), state);
    g_signal_connect(state->btn_minimal, "toggled", G_CALLBACK(on_style_toggled), state);
    
    gtk_box_append(GTK_BOX(gen_left), gtk_label_new("Upload Logo (optional):"));
    GtkWidget *logo_drop = gtk_button_new_with_label("Click to upload or drag and drop\nPNG, JPG, SVG up to 10MB");
    gtk_widget_add_css_class(logo_drop, "flat");
    g_signal_connect(logo_drop, "clicked", G_CALLBACK(on_upload_logo), state);
    GtkDropTarget *logo_dt = gtk_drop_target_new(GDK_TYPE_FILE_LIST, GDK_ACTION_COPY);
    g_signal_connect(logo_dt, "drop", G_CALLBACK(on_logo_drop), state);
    gtk_widget_add_controller(logo_drop, GTK_EVENT_CONTROLLER(logo_dt));
    gtk_box_append(GTK_BOX(gen_left), logo_drop);
    
    GtkWidget *gen_btn = gtk_button_new_with_label("Generate QR Code");
    gtk_widget_add_css_class(gen_btn, "suggested-action");
    gtk_widget_set_margin_top(gen_btn, 16);
    g_signal_connect(gen_btn, "clicked", G_CALLBACK(on_generate_clicked), state);
    gtk_box_append(GTK_BOX(gen_left), gen_btn);
    
    // Right: Gen Preview
    GtkWidget *gen_right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_size_request(gen_right, 300, -1);
    
    GtkWidget *prev_lbl = gtk_label_new("QR Code Preview");
    gtk_widget_add_css_class(prev_lbl, "heading");
    gtk_box_append(GTK_BOX(gen_right), prev_lbl);
    
    state->preview_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(state->preview_area, 250, 250);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(state->preview_area), on_preview_draw, state, NULL);
    gtk_widget_set_halign(state->preview_area, GTK_ALIGN_CENTER);
    
    GtkWidget *prev_bg = gtk_frame_new(NULL);
    gtk_frame_set_child(GTK_FRAME(prev_bg), state->preview_area);
    gtk_widget_add_css_class(prev_bg, "view");
    gtk_box_append(GTK_BOX(gen_right), prev_bg);
    
    gtk_box_append(GTK_BOX(gen_right), gtk_label_new("Download QR Code"));
    GtkWidget *dl_png = gtk_button_new_with_label("Download PNG");
    GtkWidget *dl_jpg = gtk_button_new_with_label("Download JPG");
    GtkWidget *dl_svg = gtk_button_new_with_label("Download SVG");
    g_signal_connect(dl_png, "clicked", G_CALLBACK(on_download_png), state);
    
    gtk_box_append(GTK_BOX(gen_right), dl_png);
    gtk_box_append(GTK_BOX(gen_right), dl_jpg);
    gtk_box_append(GTK_BOX(gen_right), dl_svg);
    
    gtk_box_append(GTK_BOX(gen_box), gen_left);
    gtk_box_append(GTK_BOX(gen_box), gtk_separator_new(GTK_ORIENTATION_VERTICAL));
    gtk_box_append(GTK_BOX(gen_box), gen_right);
    
    // ==========================================
    // SCANNER CARD
    // ==========================================
    GtkWidget *scan_frame = gtk_frame_new(NULL);
    GtkWidget *scan_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 24);
    gtk_widget_set_margin_top(scan_box, 16); gtk_widget_set_margin_bottom(scan_box, 16);
    gtk_widget_set_margin_start(scan_box, 16); gtk_widget_set_margin_end(scan_box, 16);
    gtk_frame_set_child(GTK_FRAME(scan_frame), scan_box);
    
    GtkWidget *scan_left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_hexpand(scan_left, TRUE);
    
    GtkWidget *scan_lbl = gtk_label_new("Upload QR Code Image:");
    gtk_widget_set_halign(scan_lbl, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(scan_left), scan_lbl);
    
    GtkWidget *scan_drop = gtk_button_new_with_label("Click to upload or drag and drop\nPNG, JPG, SVG up to 10MB");
    gtk_widget_set_size_request(scan_drop, -1, 150);
    g_signal_connect(scan_drop, "clicked", G_CALLBACK(on_scan_upload), state);
    GtkDropTarget *scan_dt = gtk_drop_target_new(GDK_TYPE_FILE_LIST, GDK_ACTION_COPY);
    g_signal_connect(scan_dt, "drop", G_CALLBACK(on_scan_drop), state);
    gtk_widget_add_controller(scan_drop, GTK_EVENT_CONTROLLER(scan_dt));
    gtk_box_append(GTK_BOX(scan_left), scan_drop);
    
    GtkWidget *scan_btn = gtk_button_new_with_label("Scan QR Code");
    gtk_widget_add_css_class(scan_btn, "suggested-action");
    g_signal_connect(scan_btn, "clicked", G_CALLBACK(on_scan_upload), state);
    gtk_box_append(GTK_BOX(scan_left), scan_btn);
    
    GtkWidget *scan_right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_size_request(scan_right, 300, -1);
    
    GtkWidget *res_lbl = gtk_label_new("Scan Results");
    gtk_widget_add_css_class(res_lbl, "heading");
    gtk_box_append(GTK_BOX(scan_right), res_lbl);
    
    state->scan_result = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(state->scan_result), FALSE);
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->scan_result)), 
        "\n\n\nUpload a QR code image to see the decoded content here.", -1);
    gtk_widget_set_vexpand(state->scan_result, TRUE);
    GtkWidget *res_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(res_scroll), state->scan_result);
    gtk_widget_add_css_class(res_scroll, "frame");
    gtk_box_append(GTK_BOX(scan_right), res_scroll);
    
    gtk_box_append(GTK_BOX(scan_box), scan_left);
    gtk_box_append(GTK_BOX(scan_box), gtk_separator_new(GTK_ORIENTATION_VERTICAL));
    gtk_box_append(GTK_BOX(scan_box), scan_right);
    
    // Assemble main layout
    GtkWidget *gen_title = gtk_label_new("QR Code Generator");
    gtk_widget_add_css_class(gen_title, "heading");
    gtk_widget_set_halign(gen_title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(main_vbox), gen_title);
    gtk_box_append(GTK_BOX(main_vbox), gen_frame);
    
    gtk_box_append(GTK_BOX(main_vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    
    GtkWidget *scan_title = gtk_label_new("QR Code Scanner");
    gtk_widget_add_css_class(scan_title, "heading");
    gtk_widget_set_halign(scan_title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(main_vbox), scan_title);
    gtk_box_append(GTK_BOX(main_vbox), scan_frame);
    
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), main_vbox);
    return scroll;
}
