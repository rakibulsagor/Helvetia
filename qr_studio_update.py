import re

with open('src/modules/utility/qr_studio.c', 'r') as f:
    text = f.read()

# Add include
text = text.replace('#include "qrcodegen.h"', '#include "qrcodegen.h"\n#include "quirc/quirc.h"')

# Add new functions before on_download_png (around line 196)
idx = text.find('static void on_download_png')
if idx == -1:
    print("Could not find on_download_png")
    exit(1)

new_funcs = """
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

"""

text = text[:idx] + new_funcs + text[idx:]

# Attach controllers
old_logo_drop = '''    GtkWidget *logo_drop = gtk_button_new_with_label("Click to upload or drag and drop\\nPNG, JPG, SVG up to 10MB");
    gtk_widget_add_css_class(logo_drop, "flat");
    g_signal_connect(logo_drop, "clicked", G_CALLBACK(on_upload_logo), state);
    gtk_box_append(GTK_BOX(gen_left), logo_drop);'''
new_logo_drop = '''    GtkWidget *logo_drop = gtk_button_new_with_label("Click to upload or drag and drop\\nPNG, JPG, SVG up to 10MB");
    gtk_widget_add_css_class(logo_drop, "flat");
    g_signal_connect(logo_drop, "clicked", G_CALLBACK(on_upload_logo), state);
    GtkDropTarget *logo_dt = gtk_drop_target_new(GDK_TYPE_FILE_LIST, GDK_ACTION_COPY);
    g_signal_connect(logo_dt, "drop", G_CALLBACK(on_logo_drop), state);
    gtk_widget_add_controller(logo_drop, GTK_EVENT_CONTROLLER(logo_dt));
    gtk_box_append(GTK_BOX(gen_left), logo_drop);'''
text = text.replace(old_logo_drop, new_logo_drop)

old_scan_drop = '''    GtkWidget *scan_drop = gtk_button_new_with_label("Click to upload or drag and drop\\nPNG, JPG, SVG up to 10MB");
    gtk_widget_set_size_request(scan_drop, -1, 150);
    gtk_box_append(GTK_BOX(scan_left), scan_drop);'''
new_scan_drop = '''    GtkWidget *scan_drop = gtk_button_new_with_label("Click to upload or drag and drop\\nPNG, JPG, SVG up to 10MB");
    gtk_widget_set_size_request(scan_drop, -1, 150);
    g_signal_connect(scan_drop, "clicked", G_CALLBACK(on_scan_upload), state);
    GtkDropTarget *scan_dt = gtk_drop_target_new(GDK_TYPE_FILE_LIST, GDK_ACTION_COPY);
    g_signal_connect(scan_dt, "drop", G_CALLBACK(on_scan_drop), state);
    gtk_widget_add_controller(scan_drop, GTK_EVENT_CONTROLLER(scan_dt));
    gtk_box_append(GTK_BOX(scan_left), scan_drop);'''
text = text.replace(old_scan_drop, new_scan_drop)

# Wire scan button to the file dialog as well
old_scan_btn = '''    GtkWidget *scan_btn = gtk_button_new_with_label("Scan QR Code");
    gtk_widget_add_css_class(scan_btn, "suggested-action");
    gtk_box_append(GTK_BOX(scan_left), scan_btn);'''
new_scan_btn = '''    GtkWidget *scan_btn = gtk_button_new_with_label("Scan QR Code");
    gtk_widget_add_css_class(scan_btn, "suggested-action");
    g_signal_connect(scan_btn, "clicked", G_CALLBACK(on_scan_upload), state);
    gtk_box_append(GTK_BOX(scan_left), scan_btn);'''
text = text.replace(old_scan_btn, new_scan_btn)

with open('src/modules/utility/qr_studio.c', 'w') as f:
    f.write(text)

