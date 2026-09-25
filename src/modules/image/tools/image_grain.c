#include <gtk/gtk.h>
#include <adwaita.h>
#include <math.h>
#include <stdlib.h>
#include "../image_shared.h"
#include "image_grain.h"

/* ================================================================== */
/* SHARED STATE                                                       */
/* ================================================================== */

typedef struct {
    GdkPixbuf *original;
    GdkPixbuf *preview;
    GdkPixbuf *first_original;
    GPtrArray *undo_stack;
    char      *path;

    /* Film Grain */
    double     grain_intensity;   /* 0..100 */
    double     grain_size;        /* 1..10 */
    gboolean   grain_mono;        /* TRUE = single-channel noise */

    /* Glow */
    double     glow_radius;       /* 1..40 */
    double     glow_intensity;    /* 0..100 */
    double     glow_threshold;    /* 0..255 */

    /* Emboss */
    double     emboss_strength;   /* 0..100 */
    int        emboss_direction;  /* 0..7 (8 directions) */

    /* Edge Detect */
    int        edge_algo;         /* 0=sobel, 1=prewitt, 2=laplacian */
    double     edge_strength;     /* 0..100 */
    gboolean   edge_invert;       /* TRUE = black edges on white */

    /* Pixelate */
    int        pixel_block;       /* 2..64 */
    gboolean   pixel_square;      /* TRUE = square, FALSE = average blend */

    /* Mosaic */
    int        mosaic_block;      /* 4..64 */
    int        mosaic_pattern;    /* 0=rect, 1=hex, 2=brick */

    double     zoom;
    GtkWidget *stack, *picture, *root;
    GtkWidget *undo_btn;
} GrainState;

static GrainState *get_state(GtkWidget *v) {
    return g_object_get_data(G_OBJECT(v), "grain-state");
}

/* ================================================================== */
/* UNDO                                                               */
/* ================================================================== */

static void push_snapshot(GrainState *st) {
    double *s = g_new0(double, 16);
    s[0]  = st->grain_intensity;
    s[1]  = st->grain_size;
    s[2]  = st->grain_mono ? 1 : 0;
    s[3]  = st->glow_radius;
    s[4]  = st->glow_intensity;
    s[5]  = st->glow_threshold;
    s[6]  = st->emboss_strength;
    s[7]  = st->emboss_direction;
    s[8]  = st->edge_algo;
    s[9]  = st->edge_strength;
    s[10] = st->edge_invert ? 1 : 0;
    s[11] = st->pixel_block;
    s[12] = st->pixel_square ? 1 : 0;
    s[13] = st->mosaic_block;
    s[14] = st->mosaic_pattern;
    g_ptr_array_add(st->undo_stack, s);
    gtk_widget_set_sensitive(st->undo_btn, TRUE);
}

static void on_undo(GtkButton *b, gpointer d) {
    (void)b;
    GrainState *st = get_state(d);
    if (st->undo_stack->len == 0) return;
    double *s = g_ptr_array_index(st->undo_stack, st->undo_stack->len - 1);
    g_ptr_array_remove_index(st->undo_stack, st->undo_stack->len - 1);
    st->grain_intensity = s[0];
    st->grain_size = s[1];
    st->grain_mono = s[2] > 0.5;
    st->glow_radius = s[3];
    st->glow_intensity = s[4];
    st->glow_threshold = s[5];
    st->emboss_strength = s[6];
    st->emboss_direction = (int)s[7];
    st->edge_algo = (int)s[8];
    st->edge_strength = s[9];
    st->edge_invert = s[10] > 0.5;
    st->pixel_block = (int)s[11];
    st->pixel_square = s[12] > 0.5;
    st->mosaic_block = (int)s[13];
    st->mosaic_pattern = (int)s[14];
    g_free(s);
    gtk_widget_set_sensitive(st->undo_btn, st->undo_stack->len > 0);
}

static void clear_undo(GrainState *st) {
    for (guint i = 0; i < st->undo_stack->len; i++)
        g_free(g_ptr_array_index(st->undo_stack, i));
    g_ptr_array_set_size(st->undo_stack, 0);
    gtk_widget_set_sensitive(st->undo_btn, FALSE);
}

/* ================================================================== */
/* SHARED HELPERS                                                     */
/* ================================================================== */

static void update_preview(GrainState *st) {
    GdkPixbuf *src = st->preview ? st->preview : st->original;
    if (!src) return;
    gtk_picture_set_paintable(GTK_PICTURE(st->picture), GDK_PAINTABLE(gdk_texture_new_for_pixbuf(src)));
    gtk_picture_set_content_fit(GTK_PICTURE(st->picture),
                                 GTK_CONTENT_FIT_CONTAIN);
    gtk_picture_set_can_shrink(GTK_PICTURE(st->picture), TRUE);
    
}

static void on_save_common(GrainState *st, const char *prefix) {
    GdkPixbuf *src = st->preview ? st->preview : st->original;
    if (!src) return;
    char *base = st->path ? g_path_get_basename(st->path) : g_strdup("image.png");
    char *name = g_strdup_printf("%s_%s", prefix, base);
    image_save_pixbuf_dialog(st->root, src, name);
    g_free(name);
    g_free(base);
}

static void on_drop_common(GrainState *st, const char *path) {
    st->zoom = 1.0;
    GError *e = NULL;
    GdkPixbuf *pb = image_load_any(path, &e);
    if (!pb) { image_show_error(st->root, e->message); g_error_free(e); return; }

    g_clear_object(&st->original);
    g_clear_object(&st->preview);
    g_clear_object(&st->first_original);
    g_free(st->path);
    clear_undo(st);

    st->original = pb;
    st->first_original = gdk_pixbuf_copy(pb);
    st->path = g_strdup(path);

    /* Defaults */
    st->grain_intensity = 30;
    st->grain_size = 2;
    st->grain_mono = TRUE;
    st->glow_radius = 15;
    st->glow_intensity = 50;
    st->glow_threshold = 180;
    st->emboss_strength = 100;
    st->emboss_direction = 0;
    st->edge_algo = 0;
    st->edge_strength = 100;
    st->edge_invert = FALSE;
    st->pixel_block = 8;
    st->pixel_square = TRUE;
    st->mosaic_block = 12;
    st->mosaic_pattern = 0;

    update_preview(st);
    gtk_stack_set_visible_child_name(GTK_STACK(st->stack), "editor");
}

static void grain_state_free(GrainState *st) {
    if (!st) return;
    g_clear_object(&st->original);
    g_clear_object(&st->preview);
    g_clear_object(&st->first_original);
    if (st->undo_stack) {
        for (guint i = 0; i < st->undo_stack->len; i++)
            g_free(g_ptr_array_index(st->undo_stack, i));
        g_ptr_array_unref(st->undo_stack);
    }
    g_free(st->path);
    g_free(st);
}

static inline double clampd(double v, double lo, double hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/* ================================================================== */
/* FILM GRAIN                                                         */
/* ================================================================== */

/* Deterministic pseudo-random based on position */
static inline guint32 hash_pos(guint32 x, guint32 y) {
    guint32 h = x * 374761393u + y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static GdkPixbuf *apply_film_grain(GrainState *st) {
    GdkPixbuf *src = st->original;
    int w = gdk_pixbuf_get_width(src);
    int h = gdk_pixbuf_get_height(src);
    int n = gdk_pixbuf_get_n_channels(src);
    int stride = gdk_pixbuf_get_rowstride(src);
    guchar *px = gdk_pixbuf_get_pixels(src);

    double intensity = st->grain_intensity / 100.0 * 60.0;   /* max ±60 */
    int grain_size = (int)(st->grain_size + 0.5);
    if (grain_size < 1) grain_size = 1;

    GdkPixbuf *out = gdk_pixbuf_copy(src);
    int ostride = gdk_pixbuf_get_rowstride(out);
    guchar *opx = gdk_pixbuf_get_pixels(out);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            guchar *sp = px + y * stride + x * n;
            guchar *dp = opx + y * ostride + x * n;

            /* Cell coordinates for grain size */
            guint32 cx = (guint32)(x / grain_size);
            guint32 cy = (guint32)(y / grain_size);

            /* Per-pixel noise from hash */
            guint32 r = hash_pos(cx, cy);
            /* Convert to [-1, 1] */
            double nr = ((double)(r & 0xFFFF) / 32767.5) - 1.0;

            if (st->grain_mono) {
                /* Same offset to all channels */
                int noise = (int)(nr * intensity);
                dp[0] = (guchar)clampd(sp[0] + noise + 0.5, 0, 255);
                dp[1] = (guchar)clampd(sp[1] + noise + 0.5, 0, 255);
                dp[2] = (guchar)clampd(sp[2] + noise + 0.5, 0, 255);
            } else {
                /* Independent per-channel noise */
                guint32 g1 = hash_pos(cx, cy ^ 0x1234);
                guint32 b1 = hash_pos(cx ^ 0x5678, cy);
                double ng = ((double)(g1 & 0xFFFF) / 32767.5) - 1.0;
                double nb = ((double)(b1 & 0xFFFF) / 32767.5) - 1.0;
                dp[0] = (guchar)clampd(sp[0] + nr * intensity + 0.5, 0, 255);
                dp[1] = (guchar)clampd(sp[1] + ng * intensity + 0.5, 0, 255);
                dp[2] = (guchar)clampd(sp[2] + nb * intensity + 0.5, 0, 255);
            }
            if (n == 4) dp[3] = sp[3];
        }
    }
    return out;
}

/* ================================================================== */
/* GLOW — blur bright areas, add back                                    */
/* ================================================================== */

static GdkPixbuf *apply_glow(GrainState *st) {
    GdkPixbuf *src = st->original;
    int w = gdk_pixbuf_get_width(src);
    int h = gdk_pixbuf_get_height(src);
    int n = gdk_pixbuf_get_n_channels(src);
    int stride = gdk_pixbuf_get_rowstride(src);
    guchar *px = gdk_pixbuf_get_pixels(src);

    int radius = (int)(st->glow_radius + 0.5);
    if (radius < 1) radius = 1;
    if (radius > 40) radius = 40;

    double intensity = st->glow_intensity / 100.0;
    double threshold = st->glow_threshold;

    /* Extract bright pixels into a mask, then box-blur the mask */
    int sz = w * h;
    double *bright = g_new0(double, sz);
    double *blurred = g_new0(double, sz);
    double *tmp = g_new(double, sz);

    for (int i = 0; i < sz; i++) {
        int y = i / w, x = i % w;
        guchar *sp = px + y * stride + x * n;
        double Y = 0.2126 * sp[0] + 0.7152 * sp[1] + 0.0722 * sp[2];
        bright[i] = Y > threshold ? (Y - threshold) : 0;
    }

    /* Two-pass box blur of the bright mask */
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            double sum = 0; int cnt = 0;
            for (int k = -radius; k <= radius; k++) {
                int nx = x + k;
                if (nx < 0 || nx >= w) continue;
                sum += bright[y * w + nx];
                cnt++;
            }
            tmp[y * w + x] = cnt ? sum / cnt : 0;
        }
    }
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            double sum = 0; int cnt = 0;
            for (int k = -radius; k <= radius; k++) {
                int ny = y + k;
                if (ny < 0 || ny >= h) continue;
                sum += tmp[ny * w + x];
                cnt++;
            }
            blurred[y * w + x] = cnt ? sum / cnt : 0;
        }
    }

    GdkPixbuf *out = gdk_pixbuf_copy(src);
    int ostride = gdk_pixbuf_get_rowstride(out);
    guchar *opx = gdk_pixbuf_get_pixels(out);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            guchar *sp = px + y * stride + x * n;
            guchar *dp = opx + y * ostride + x * n;
            double glow = blurred[y * w + x] * intensity;
            dp[0] = (guchar)clampd(sp[0] + glow + 0.5, 0, 255);
            dp[1] = (guchar)clampd(sp[1] + glow + 0.5, 0, 255);
            dp[2] = (guchar)clampd(sp[2] + glow + 0.5, 0, 255);
            if (n == 4) dp[3] = sp[3];
        }
    }

    g_free(bright);
    g_free(blurred);
    g_free(tmp);
    return out;
}

/* ================================================================== */
/* EMBOSS                                                             */
/* ================================================================== */

static GdkPixbuf *apply_emboss(GrainState *st) {
    GdkPixbuf *src = st->original;
    int w = gdk_pixbuf_get_width(src);
    int h = gdk_pixbuf_get_height(src);
    int n = gdk_pixbuf_get_n_channels(src);
    int stride = gdk_pixbuf_get_rowstride(src);
    guchar *px = gdk_pixbuf_get_pixels(src);

    double strength = st->emboss_strength / 100.0;

    /* 8-direction emboss kernels */
    static const int kernels[8][9] = {
        /* N  */ { 1, 1, 1, 0, 0, 0, -1, -1, -1 },
        /* NE */ { 1, 1, 0, 1, 0, -1, 0, -1, -1 },
        /* E  */ { 1, 0, -1, 1, 0, -1, 1, 0, -1 },
        /* SE */ { 0, 1, 1, -1, 0, 1, -1, -1, 0 },
        /* S  */ { -1, -1, -1, 0, 0, 0, 1, 1, 1 },
        /* SW */ { -1, -1, 0, -1, 0, 1, 0, 1, 1 },
        /* W  */ { -1, 0, 1, -1, 0, 1, -1, 0, 1 },
        /* NW */ { 0, -1, -1, 1, 0, -1, 1, 1, 0 },
    };
    int dir = st->emboss_direction;
    if (dir < 0) dir = 0;
    if (dir > 7) dir = 7;
    const int *k = kernels[dir];

    GdkPixbuf *out = gdk_pixbuf_copy(src);
    int ostride = gdk_pixbuf_get_rowstride(out);
    guchar *opx = gdk_pixbuf_get_pixels(out);

    /* Start with a mid-gray background */
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            guchar *dp = opx + y * ostride + x * n;
            dp[0] = dp[1] = dp[2] = 128;
        }
    }

    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            for (int c = 0; c < 3; c++) {
                double sum = 0;
                int idx = 0;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        int v = px[(y + dy) * stride + (x + dx) * n + c];
                        sum += k[idx++] * v;
                    }
                }
                double outv = 128.0 + sum * strength;
                opx[y * ostride + x * n + c] =
                    (guchar)clampd(outv + 0.5, 0, 255);
            }
            if (n == 4)
                opx[y * ostride + x * n + 3] = px[y * stride + x * n + 3];
        }
    }
    return out;
}

/* ================================================================== */
/* EDGE DETECT                                                        */
/* ================================================================== */

static GdkPixbuf *apply_edge_detect(GrainState *st) {
    GdkPixbuf *src = st->original;
    int w = gdk_pixbuf_get_width(src);
    int h = gdk_pixbuf_get_height(src);
    int n = gdk_pixbuf_get_n_channels(src);
    int stride = gdk_pixbuf_get_rowstride(src);
    guchar *px = gdk_pixbuf_get_pixels(src);

    double strength = st->edge_strength / 100.0;

    GdkPixbuf *out = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, w, h);
    int ostride = gdk_pixbuf_get_rowstride(out);
    guchar *opx = gdk_pixbuf_get_pixels(out);

    /* Grayscale first */
    double *gray = g_new(double, w * h);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            guchar *sp = px + y * stride + x * n;
            gray[y * w + x] = 0.2126 * sp[0] + 0.7152 * sp[1] + 0.0722 * sp[2];
        }

    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            double gx = 0, gy = 0;

            if (st->edge_algo == 0) {
                /* Sobel */
                gx = -gray[(y-1)*w + (x-1)] + gray[(y-1)*w + (x+1)]
                     - 2*gray[y*w + (x-1)] + 2*gray[y*w + (x+1)]
                     - gray[(y+1)*w + (x-1)] + gray[(y+1)*w + (x+1)];
                gy = -gray[(y-1)*w + (x-1)] - 2*gray[(y-1)*w + x] - gray[(y-1)*w + (x+1)]
                     + gray[(y+1)*w + (x-1)] + 2*gray[(y+1)*w + x] + gray[(y+1)*w + (x+1)];
            } else if (st->edge_algo == 1) {
                /* Prewitt */
                gx = -gray[(y-1)*w + (x-1)] + gray[(y-1)*w + (x+1)]
                     - gray[y*w + (x-1)] + gray[y*w + (x+1)]
                     - gray[(y+1)*w + (x-1)] + gray[(y+1)*w + (x+1)];
                gy = -gray[(y-1)*w + (x-1)] - gray[(y-1)*w + x] - gray[(y-1)*w + (x+1)]
                     + gray[(y+1)*w + (x-1)] + gray[(y+1)*w + x] + gray[(y+1)*w + (x+1)];
            } else {
                /* Laplacian */
                gx = 4*gray[y*w + x]
                     - gray[(y-1)*w + x] - gray[(y+1)*w + x]
                     - gray[y*w + (x-1)] - gray[y*w + (x+1)];
                gy = 0;
            }

            double mag = sqrt(gx*gx + gy*gy) * strength;
            if (st->edge_algo == 2) mag = fabs(gx) * strength;
            if (mag > 255) mag = 255;

            guchar v;
            if (st->edge_invert) {
                v = (guchar)(255 - mag);
            } else {
                v = (guchar)mag;
            }
            opx[y * ostride + x * 3 + 0] = v;
            opx[y * ostride + x * 3 + 1] = v;
            opx[y * ostride + x * 3 + 2] = v;
        }
    }

    /* Handle edges — copy from original (grayscale) */
    for (int x = 0; x < w; x++) {
        double v = gray[x];
        for (int c = 0; c < 3; c++)
            opx[0 * ostride + x * 3 + c] = (guchar)v;
    }
    /* Simpler: fill border with black or white */
    guchar edge_val = st->edge_invert ? 255 : 0;
    for (int x = 0; x < w; x++) {
        for (int c = 0; c < 3; c++) {
            opx[0 * ostride + x * 3 + c] = edge_val;
            opx[(h-1) * ostride + x * 3 + c] = edge_val;
        }
    }
    for (int y = 0; y < h; y++) {
        for (int c = 0; c < 3; c++) {
            opx[y * ostride + 0 * 3 + c] = edge_val;
            opx[y * ostride + (w-1) * 3 + c] = edge_val;
        }
    }

    g_free(gray);
    return out;
}

/* ================================================================== */
/* PIXELATE                                                           */
/* ================================================================== */

static GdkPixbuf *apply_pixelate(GrainState *st) {
    GdkPixbuf *src = st->original;
    int w = gdk_pixbuf_get_width(src);
    int h = gdk_pixbuf_get_height(src);
    int n = gdk_pixbuf_get_n_channels(src);
    int stride = gdk_pixbuf_get_rowstride(src);
    guchar *px = gdk_pixbuf_get_pixels(src);

    int block = st->pixel_block;
    if (block < 2) block = 2;
    if (block > 64) block = 64;

    GdkPixbuf *out = gdk_pixbuf_copy(src);
    int ostride = gdk_pixbuf_get_rowstride(out);
    guchar *opx = gdk_pixbuf_get_pixels(out);

    for (int by = 0; by < h; by += block) {
        for (int bx = 0; bx < w; bx += block) {
            /* Average colors in this block */
            long sum_r = 0, sum_g = 0, sum_b = 0, sum_a = 0;
            int cnt = 0;
            int max_y = (by + block < h) ? by + block : h;
            int max_x = (bx + block < w) ? bx + block : w;

            for (int y = by; y < max_y; y++) {
                for (int x = bx; x < max_x; x++) {
                    guchar *sp = px + y * stride + x * n;
                    sum_r += sp[0];
                    sum_g += sp[1];
                    sum_b += sp[2];
                    if (n == 4) sum_a += sp[3];
                    cnt++;
                }
            }
            if (cnt == 0) continue;

            guchar ar = sum_r / cnt;
            guchar ag = sum_g / cnt;
            guchar ab = sum_b / cnt;
            guchar aa = (n == 4) ? sum_a / cnt : 255;

            /* Fill the block */
            for (int y = by; y < max_y; y++) {
                for (int x = bx; x < max_x; x++) {
                    guchar *dp = opx + y * ostride + x * n;
                    if (st->pixel_square) {
                        /* Hard square */
                        dp[0] = ar; dp[1] = ag; dp[2] = ab;
                        if (n == 4) dp[3] = aa;
                    } else {
                        /* Blend toward the block average */
                        guchar *sp = px + y * stride + x * n;
                        int blend = 200;   /* 200/255 ≈ 78% block, 22% original */
                        dp[0] = (sp[0] * (255 - blend) + ar * blend) / 255;
                        dp[1] = (sp[1] * (255 - blend) + ag * blend) / 255;
                        dp[2] = (sp[2] * (255 - blend) + ab * blend) / 255;
                        if (n == 4) dp[3] = sp[3];
                    }
                }
            }
        }
    }
    return out;
}

/* ================================================================== */
/* MOSAIC                                                             */
/* ================================================================== */

static GdkPixbuf *apply_mosaic(GrainState *st) {
    GdkPixbuf *src = st->original;
    int w = gdk_pixbuf_get_width(src);
    int h = gdk_pixbuf_get_height(src);
    int n = gdk_pixbuf_get_n_channels(src);
    int stride = gdk_pixbuf_get_rowstride(src);
    guchar *px = gdk_pixbuf_get_pixels(src);

    int block = st->mosaic_block;
    if (block < 4) block = 4;
    if (block > 64) block = 64;

    GdkPixbuf *out = gdk_pixbuf_copy(src);
    int ostride = gdk_pixbuf_get_rowstride(out);
    guchar *opx = gdk_pixbuf_get_pixels(out);

    for (int by = 0; by < h; by += block) {
        /* Brick pattern shifts every other row */
        int row_offset = 0;
        if (st->mosaic_pattern == 2 && ((by / block) % 2 == 1))
            row_offset = block / 2;

        for (int bx = -row_offset; bx < w; bx += block) {
            long sum_r = 0, sum_g = 0, sum_b = 0;
            int cnt = 0;
            int max_y = (by + block < h) ? by + block : h;
            int min_x = (bx < 0) ? 0 : bx;
            int max_x = (bx + block < w) ? bx + block : w;

            for (int y = by; y < max_y; y++) {
                for (int x = min_x; x < max_x; x++) {
                    guchar *sp = px + y * stride + x * n;
                    sum_r += sp[0];
                    sum_g += sp[1];
                    sum_b += sp[2];
                    cnt++;
                }
            }
            if (cnt == 0) continue;

            guchar ar = sum_r / cnt;
            guchar ag = sum_g / cnt;
            guchar ab = sum_b / cnt;

            /* Hex pattern — approximate with hexagonal clipping mask */
            if (st->mosaic_pattern == 1) {
                /* Draw as rounded diamond/hex shape */
                double cx_block = bx + block / 2.0;
                double cy_block = by + block / 2.0;
                double r = block / 2.0;

                for (int y = by; y < max_y; y++) {
                    for (int x = min_x; x < max_x; x++) {
                        double dx = fabs(x - cx_block);
                        double dy = fabs(y - cy_block);
                        /* Approximate hex: diamond-ish mask */
                        int in_hex = (dx + dy * 0.8 <= r * 1.1);
                        guchar *dp = opx + y * ostride + x * n;
                        if (in_hex) {
                            dp[0] = ar; dp[1] = ag; dp[2] = ab;
                        }
                    }
                }
            } else {
                for (int y = by; y < max_y; y++) {
                    for (int x = min_x; x < max_x; x++) {
                        guchar *dp = opx + y * ostride + x * n;
                        dp[0] = ar;
                        dp[1] = ag;
                        dp[2] = ab;
                    }
                }
            }
            if (n == 4) {
                for (int y = by; y < max_y; y++)
                    for (int x = min_x; x < max_x; x++)
                        opx[y * ostride + x * n + 3] = px[y * stride + x * n + 3];
            }
        }
    }
    return out;
}

/* ================================================================== */
/* UI HELPERS                                                         */
/* ================================================================== */

static GtkWidget *make_slider(const char *label, double min, double max,
                                double step, double initial,
                                GtkWidget **out_scale, GtkWidget **out_lbl,
                                GCallback cb, gpointer user_data) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *lbl = gtk_label_new(label);
    gtk_widget_add_css_class(lbl, "dim-label");
    gtk_widget_set_size_request(lbl, 130, -1);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);

    GtkWidget *scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,
                                                 min, max, step);
    gtk_widget_set_size_request(scale, 220, -1);
    gtk_range_set_value(GTK_RANGE(scale), initial);
    gtk_scale_set_draw_value(GTK_SCALE(scale), FALSE);
    gtk_widget_set_hexpand(scale, TRUE);

    GtkWidget *val = gtk_label_new("");
    gtk_widget_set_size_request(val, 60, -1);
    gtk_label_set_xalign(GTK_LABEL(val), 1.0f);
    gtk_widget_add_css_class(val, "dim-label");

    gtk_box_append(GTK_BOX(row), lbl);
    gtk_box_append(GTK_BOX(row), scale);
    gtk_box_append(GTK_BOX(row), val);

    *out_scale = scale;
    *out_lbl = val;
    if (cb) g_signal_connect(scale, "value-changed", cb, user_data);
    return row;
}

static GtkWidget *make_switch_row(const char *label, gboolean initial,
                                    GCallback cb, gpointer user_data) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *lbl = gtk_label_new(label);
    gtk_widget_set_size_request(lbl, 200, -1);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
    gtk_widget_set_hexpand(lbl, TRUE);

    GtkWidget *sw = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(sw), initial);
    gtk_widget_set_valign(sw, GTK_ALIGN_CENTER);
    if (cb) g_signal_connect(sw, "notify::active", cb, user_data);

    gtk_box_append(GTK_BOX(row), lbl);
    gtk_box_append(GTK_BOX(row), sw);
    return row;
}

static GtkWidget *make_dropdown_row(const char *label,
                                     const char **items,
                                     int initial,
                                     GCallback cb, gpointer user_data) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *lbl = gtk_label_new(label);
    gtk_widget_add_css_class(lbl, "dim-label");
    gtk_widget_set_size_request(lbl, 130, -1);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);

    GtkWidget *dd = gtk_drop_down_new_from_strings(items);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(dd), initial);
    gtk_widget_set_hexpand(dd, TRUE);
    if (cb) g_signal_connect(dd, "notify::selected", cb, user_data);

    gtk_box_append(GTK_BOX(row), lbl);
    gtk_box_append(GTK_BOX(row), dd);
    return row;
}

static GtkWidget *build_shell(GrainState *st, GtkWidget **out_sliders,
                                const char *drop_hint,
                                ImageDropCallback on_drop,
                                GCallback on_save,
                                GCallback on_reset,
                                gpointer root) {
    GtkWidget *stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(stack),
                                  GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_widget_set_vexpand(stack, TRUE);
    st->stack = stack;

    GtkWidget *drop_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(drop_box, 24);
    gtk_widget_set_margin_end(drop_box, 24);
    gtk_widget_set_margin_top(drop_box, 24);
    gtk_widget_set_margin_bottom(drop_box, 24);
    gtk_widget_set_vexpand(drop_box, TRUE);
    gtk_box_append(GTK_BOX(drop_box),
        image_build_drop_zone(drop_hint, on_drop, root));
    gtk_stack_add_named(GTK_STACK(stack), drop_box, "drop");

    GtkWidget *editor = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    GtkWidget *sliders = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start(sliders, 12);
    gtk_widget_set_margin_end(sliders, 12);
    gtk_widget_set_margin_top(sliders, 8);
    gtk_widget_set_margin_bottom(sliders, 8);
    *out_sliders = sliders;

    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(bar, 12);
    gtk_widget_set_margin_end(bar, 12);
    gtk_widget_set_margin_bottom(bar, 8);

    st->undo_btn = image_undo_button(G_CALLBACK(on_undo), root);
    gtk_widget_set_sensitive(st->undo_btn, FALSE);

    GtkWidget *reset = image_reset_button(on_reset, root);
    GtkWidget *new_img = image_new_image_button(on_drop, root);

    GtkWidget *sp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(sp, TRUE);

    GtkWidget *save = gtk_button_new_with_label("Save As…");
    gtk_widget_add_css_class(save, "flat");
    if (on_save) g_signal_connect(save, "clicked", on_save, root);

    gtk_box_append(GTK_BOX(bar), new_img);
    gtk_box_append(GTK_BOX(bar), gtk_separator_new(GTK_ORIENTATION_VERTICAL));
    gtk_box_append(GTK_BOX(bar), st->undo_btn);
    gtk_box_append(GTK_BOX(bar), reset);

    gtk_box_append(GTK_BOX(bar), gtk_separator_new(GTK_ORIENTATION_VERTICAL));
    GtkWidget *z_out = gtk_button_new_from_icon_name("zoom-out-symbolic");
    GtkWidget *z_in = gtk_button_new_from_icon_name("zoom-in-symbolic");
    GtkWidget *z_1 = gtk_button_new_from_icon_name("zoom-original-symbolic");
    gtk_widget_add_css_class(z_out, "flat");
    gtk_widget_add_css_class(z_in, "flat");
    gtk_widget_add_css_class(z_1, "flat");
    g_signal_connect_swapped(z_out, "clicked", G_CALLBACK(image_zoom_out), root);
    g_signal_connect_swapped(z_in, "clicked", G_CALLBACK(image_zoom_in), root);
    g_signal_connect_swapped(z_1, "clicked", G_CALLBACK(image_zoom_reset), root);
    gtk_box_append(GTK_BOX(bar), z_out);
    gtk_box_append(GTK_BOX(bar), z_1);
    gtk_box_append(GTK_BOX(bar), z_in);

    gtk_box_append(GTK_BOX(bar), sp);
    gtk_box_append(GTK_BOX(bar), save);

    GtkWidget *pic = gtk_picture_new();
    gtk_widget_set_vexpand(pic, TRUE);
    gtk_widget_set_halign(pic, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(pic, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class(pic, "image-viewer-canvas");
    st->picture = pic;

    gtk_box_append(GTK_BOX(editor), sliders);
    gtk_box_append(GTK_BOX(editor), bar);
    gtk_box_append(GTK_BOX(editor),
                   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(editor), pic);
    gtk_stack_add_named(GTK_STACK(stack), editor, "editor");
    gtk_stack_set_visible_child_name(GTK_STACK(stack), "drop");

    return stack;
}

/* ================================================================== */
/* TOOL 1 — FILM GRAIN                                                */
/* ================================================================== */

static void fg_refresh(GrainState *st) {
    g_clear_object(&st->preview);
    st->preview = apply_film_grain(st);
    update_preview(st);
}

static void fg_on_intensity(GtkRange *r, gpointer d) {
    GrainState *st = get_state(d);
    double v = gtk_range_get_value(r);
    if (fabs(v - st->grain_intensity) < 0.5) return;
    push_snapshot(st);
    st->grain_intensity = v;
    fg_refresh(st);
}
static void fg_on_size(GtkRange *r, gpointer d) {
    GrainState *st = get_state(d);
    st->grain_size = gtk_range_get_value(r);
    fg_refresh(st);
}
static void fg_on_mono(GObject *sw, GParamSpec *p, gpointer d) {
    (void)p;
    GrainState *st = get_state(d);
    st->grain_mono = gtk_switch_get_active(GTK_SWITCH(sw));
    fg_refresh(st);
}
static void fg_on_save(GtkButton *b, gpointer d) {
    (void)b; on_save_common(get_state(d), "grain");
}
static void fg_on_drop(const char *path, gpointer d) {
    GrainState *st = get_state(d);
    on_drop_common(st, path);
    if (st->original) fg_refresh(st);
}
static void fg_on_reset(GtkButton *b, gpointer d) {
    (void)b;
    GrainState *st = get_state(d);
    push_snapshot(st);
    st->grain_intensity = 30;
    st->grain_size = 2;
    st->grain_mono = TRUE;
    fg_refresh(st);
}

void image_film_grain_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "grain-state", NULL);
}
static void cmd_fg_reset(GtkWidget *v) { fg_on_reset(NULL, v); }

const HelvetiaToolCommand image_film_grain_commands[] = {
    { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic",
      .accel = NULL, .tooltip = "Reset", .activate = cmd_fg_reset },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

GtkWidget *image_film_grain_create(void) {
    GrainState *st = g_new0(GrainState, 1);
    st->undo_stack = g_ptr_array_new();
    st->zoom = 1.0;
    st->grain_intensity = 30;
    st->grain_size = 2;
    st->grain_mono = TRUE;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;

    GtkWidget *sliders;
    GtkWidget *stack = build_shell(st, &sliders, "Image file",
                                    fg_on_drop, G_CALLBACK(fg_on_save),
                                    G_CALLBACK(fg_on_reset), root);

    GtkWidget *s1, *l1, *s2, *l2;
    gtk_box_append(GTK_BOX(sliders),
        make_slider("Intensity", 0, 100, 1, 30, &s1, &l1,
                     G_CALLBACK(fg_on_intensity), root));
    gtk_box_append(GTK_BOX(sliders),
        make_slider("Grain size", 1, 10, 1, 2, &s2, &l2,
                     G_CALLBACK(fg_on_size), root));
    gtk_label_set_text(GTK_LABEL(l1), "30");
    gtk_label_set_text(GTK_LABEL(l2), "2");

    gtk_box_append(GTK_BOX(sliders),
        gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(sliders),
        make_switch_row("Monochrome grain", TRUE,
                         G_CALLBACK(fg_on_mono), root));

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "grain-state", st,
                           (GDestroyNotify)grain_state_free);
    image_register_zoom(root, st->picture, &st->zoom);
    image_install_zoom_shortcuts(root);
    return root;
}

/* ================================================================== */
/* TOOL 2 — GLOW                                                      */
/* ================================================================== */

static void gl_refresh(GrainState *st) {
    g_clear_object(&st->preview);
    st->preview = apply_glow(st);
    update_preview(st);
}

static void gl_on_radius(GtkRange *r, gpointer d) {
    GrainState *st = get_state(d);
    double v = gtk_range_get_value(r);
    if (fabs(v - st->glow_radius) < 0.5) return;
    push_snapshot(st);
    st->glow_radius = v;
    gl_refresh(st);
}
static void gl_on_intensity(GtkRange *r, gpointer d) {
    GrainState *st = get_state(d);
    st->glow_intensity = gtk_range_get_value(r);
    gl_refresh(st);
}
static void gl_on_threshold(GtkRange *r, gpointer d) {
    GrainState *st = get_state(d);
    st->glow_threshold = gtk_range_get_value(r);
    gl_refresh(st);
}
static void gl_on_save(GtkButton *b, gpointer d) {
    (void)b; on_save_common(get_state(d), "glow");
}
static void gl_on_drop(const char *path, gpointer d) {
    GrainState *st = get_state(d);
    on_drop_common(st, path);
    if (st->original) gl_refresh(st);
}
static void gl_on_reset(GtkButton *b, gpointer d) {
    (void)b;
    GrainState *st = get_state(d);
    push_snapshot(st);
    st->glow_radius = 15;
    st->glow_intensity = 50;
    st->glow_threshold = 180;
    gl_refresh(st);
}

void image_glow_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "grain-state", NULL);
}
static void cmd_gl_reset(GtkWidget *v) { gl_on_reset(NULL, v); }

const HelvetiaToolCommand image_glow_commands[] = {
    { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic",
      .accel = NULL, .tooltip = "Reset", .activate = cmd_gl_reset },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

GtkWidget *image_glow_create(void) {
    GrainState *st = g_new0(GrainState, 1);
    st->undo_stack = g_ptr_array_new();
    st->zoom = 1.0;
    st->glow_radius = 15;
    st->glow_intensity = 50;
    st->glow_threshold = 180;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;

    GtkWidget *sliders;
    GtkWidget *stack = build_shell(st, &sliders, "Image file",
                                    gl_on_drop, G_CALLBACK(gl_on_save),
                                    G_CALLBACK(gl_on_reset), root);

    GtkWidget *s1, *l1, *s2, *l2, *s3, *l3;
    gtk_box_append(GTK_BOX(sliders),
        make_slider("Radius", 1, 40, 1, 15, &s1, &l1,
                     G_CALLBACK(gl_on_radius), root));
    gtk_box_append(GTK_BOX(sliders),
        make_slider("Intensity", 0, 100, 1, 50, &s2, &l2,
                     G_CALLBACK(gl_on_intensity), root));
    gtk_box_append(GTK_BOX(sliders),
        make_slider("Threshold", 0, 255, 1, 180, &s3, &l3,
                     G_CALLBACK(gl_on_threshold), root));
    gtk_label_set_text(GTK_LABEL(l1), "15");
    gtk_label_set_text(GTK_LABEL(l2), "50");
    gtk_label_set_text(GTK_LABEL(l3), "180");

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "grain-state", st,
                           (GDestroyNotify)grain_state_free);
    image_register_zoom(root, st->picture, &st->zoom);
    image_install_zoom_shortcuts(root);
    return root;
}

/* ================================================================== */
/* TOOL 3 — EMBOSS                                                    */
/* ================================================================== */

static void em_refresh(GrainState *st) {
    g_clear_object(&st->preview);
    st->preview = apply_emboss(st);
    update_preview(st);
}

static void em_on_strength(GtkRange *r, gpointer d) {
    GrainState *st = get_state(d);
    double v = gtk_range_get_value(r);
    if (fabs(v - st->emboss_strength) < 0.5) return;
    push_snapshot(st);
    st->emboss_strength = v;
    em_refresh(st);
}
static void em_on_dir(GObject *dd, GParamSpec *p, gpointer d) {
    (void)p;
    GrainState *st = get_state(d);
    st->emboss_direction = gtk_drop_down_get_selected(GTK_DROP_DOWN(dd));
    em_refresh(st);
}
static void em_on_save(GtkButton *b, gpointer d) {
    (void)b; on_save_common(get_state(d), "emboss");
}
static void em_on_drop(const char *path, gpointer d) {
    GrainState *st = get_state(d);
    on_drop_common(st, path);
    if (st->original) em_refresh(st);
}
static void em_on_reset(GtkButton *b, gpointer d) {
    (void)b;
    GrainState *st = get_state(d);
    push_snapshot(st);
    st->emboss_strength = 100;
    st->emboss_direction = 0;
    em_refresh(st);
}

void image_emboss_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "grain-state", NULL);
}
static void cmd_em_reset(GtkWidget *v) { em_on_reset(NULL, v); }

const HelvetiaToolCommand image_emboss_commands[] = {
    { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic",
      .accel = NULL, .tooltip = "Reset", .activate = cmd_em_reset },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

GtkWidget *image_emboss_create(void) {
    GrainState *st = g_new0(GrainState, 1);
    st->undo_stack = g_ptr_array_new();
    st->zoom = 1.0;
    st->emboss_strength = 100;
    st->emboss_direction = 0;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;

    GtkWidget *sliders;
    GtkWidget *stack = build_shell(st, &sliders, "Image file",
                                    em_on_drop, G_CALLBACK(em_on_save),
                                    G_CALLBACK(em_on_reset), root);

    gtk_box_append(GTK_BOX(sliders),
        make_dropdown_row("Direction",
                          (const char *[]){"North", "North-East", "East",
                                            "South-East", "South", "South-West",
                                            "West", "North-West", NULL},
                          0, G_CALLBACK(em_on_dir), root));

    GtkWidget *s, *l;
    gtk_box_append(GTK_BOX(sliders),
        make_slider("Strength", 0, 100, 1, 100, &s, &l,
                     G_CALLBACK(em_on_strength), root));
    gtk_label_set_text(GTK_LABEL(l), "100");

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "grain-state", st,
                           (GDestroyNotify)grain_state_free);
    image_register_zoom(root, st->picture, &st->zoom);
    image_install_zoom_shortcuts(root);
    return root;
}

/* ================================================================== */
/* TOOL 4 — EDGE DETECT                                               */
/* ================================================================== */

static void ed_refresh(GrainState *st) {
    g_clear_object(&st->preview);
    st->preview = apply_edge_detect(st);
    update_preview(st);
}

static void ed_on_algo(GObject *dd, GParamSpec *p, gpointer d) {
    (void)p;
    GrainState *st = get_state(d);
    st->edge_algo = gtk_drop_down_get_selected(GTK_DROP_DOWN(dd));
    ed_refresh(st);
}
static void ed_on_strength(GtkRange *r, gpointer d) {
    GrainState *st = get_state(d);
    double v = gtk_range_get_value(r);
    if (fabs(v - st->edge_strength) < 0.5) return;
    push_snapshot(st);
    st->edge_strength = v;
    ed_refresh(st);
}
static void ed_on_invert(GObject *sw, GParamSpec *p, gpointer d) {
    (void)p;
    GrainState *st = get_state(d);
    st->edge_invert = gtk_switch_get_active(GTK_SWITCH(sw));
    ed_refresh(st);
}
static void ed_on_save(GtkButton *b, gpointer d) {
    (void)b; on_save_common(get_state(d), "edges");
}
static void ed_on_drop(const char *path, gpointer d) {
    GrainState *st = get_state(d);
    on_drop_common(st, path);
    if (st->original) ed_refresh(st);
}
static void ed_on_reset(GtkButton *b, gpointer d) {
    (void)b;
    GrainState *st = get_state(d);
    push_snapshot(st);
    st->edge_algo = 0;
    st->edge_strength = 100;
    st->edge_invert = FALSE;
    ed_refresh(st);
}

void image_edge_detect_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "grain-state", NULL);
}
static void cmd_ed_reset(GtkWidget *v) { ed_on_reset(NULL, v); }

const HelvetiaToolCommand image_edge_detect_commands[] = {
    { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic",
      .accel = NULL, .tooltip = "Reset", .activate = cmd_ed_reset },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

GtkWidget *image_edge_detect_create(void) {
    GrainState *st = g_new0(GrainState, 1);
    st->undo_stack = g_ptr_array_new();
    st->zoom = 1.0;
    st->edge_algo = 0;
    st->edge_strength = 100;
    st->edge_invert = FALSE;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;

    GtkWidget *sliders;
    GtkWidget *stack = build_shell(st, &sliders, "Image file",
                                    ed_on_drop, G_CALLBACK(ed_on_save),
                                    G_CALLBACK(ed_on_reset), root);

    gtk_box_append(GTK_BOX(sliders),
        make_dropdown_row("Algorithm",
                          (const char *[]){"Sobel", "Prewitt", "Laplacian", NULL},
                          0, G_CALLBACK(ed_on_algo), root));

    GtkWidget *s, *l;
    gtk_box_append(GTK_BOX(sliders),
        make_slider("Strength", 0, 100, 1, 100, &s, &l,
                     G_CALLBACK(ed_on_strength), root));
    gtk_label_set_text(GTK_LABEL(l), "100");

    gtk_box_append(GTK_BOX(sliders),
        gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(sliders),
        make_switch_row("Invert (white background)", FALSE,
                         G_CALLBACK(ed_on_invert), root));

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "grain-state", st,
                           (GDestroyNotify)grain_state_free);
    image_register_zoom(root, st->picture, &st->zoom);
    image_install_zoom_shortcuts(root);
    return root;
}

/* ================================================================== */
/* TOOL 5 — PIXELATE                                                  */
/* ================================================================== */

static void px_refresh(GrainState *st) {
    g_clear_object(&st->preview);
    st->preview = apply_pixelate(st);
    update_preview(st);
}

static void px_on_block(GtkRange *r, gpointer d) {
    GrainState *st = get_state(d);
    int v = (int)gtk_range_get_value(r);
    if (v == st->pixel_block) return;
    push_snapshot(st);
    st->pixel_block = v;
    px_refresh(st);
}
static void px_on_square(GObject *sw, GParamSpec *p, gpointer d) {
    (void)p;
    GrainState *st = get_state(d);
    st->pixel_square = gtk_switch_get_active(GTK_SWITCH(sw));
    px_refresh(st);
}
static void px_on_save(GtkButton *b, gpointer d) {
    (void)b; on_save_common(get_state(d), "pixelated");
}
static void px_on_drop(const char *path, gpointer d) {
    GrainState *st = get_state(d);
    on_drop_common(st, path);
    if (st->original) px_refresh(st);
}
static void px_on_reset(GtkButton *b, gpointer d) {
    (void)b;
    GrainState *st = get_state(d);
    push_snapshot(st);
    st->pixel_block = 8;
    st->pixel_square = TRUE;
    px_refresh(st);
}

void image_pixelate_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "grain-state", NULL);
}
static void cmd_px_reset(GtkWidget *v) { px_on_reset(NULL, v); }

const HelvetiaToolCommand image_pixelate_commands[] = {
    { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic",
      .accel = NULL, .tooltip = "Reset", .activate = cmd_px_reset },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

GtkWidget *image_pixelate_create(void) {
    GrainState *st = g_new0(GrainState, 1);
    st->undo_stack = g_ptr_array_new();
    st->zoom = 1.0;
    st->pixel_block = 8;
    st->pixel_square = TRUE;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;

    GtkWidget *sliders;
    GtkWidget *stack = build_shell(st, &sliders, "Image file",
                                    px_on_drop, G_CALLBACK(px_on_save),
                                    G_CALLBACK(px_on_reset), root);

    GtkWidget *s, *l;
    gtk_box_append(GTK_BOX(sliders),
        make_slider("Block size", 2, 64, 1, 8, &s, &l,
                     G_CALLBACK(px_on_block), root));
    gtk_label_set_text(GTK_LABEL(l), "8");

    gtk_box_append(GTK_BOX(sliders),
        gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(sliders),
        make_switch_row("Hard edges", TRUE,
                         G_CALLBACK(px_on_square), root));

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "grain-state", st,
                           (GDestroyNotify)grain_state_free);
    image_register_zoom(root, st->picture, &st->zoom);
    image_install_zoom_shortcuts(root);
    return root;
}

/* ================================================================== */
/* TOOL 6 — MOSAIC                                                    */
/* ================================================================== */

static void mo_refresh(GrainState *st) {
    g_clear_object(&st->preview);
    st->preview = apply_mosaic(st);
    update_preview(st);
}

static void mo_on_block(GtkRange *r, gpointer d) {
    GrainState *st = get_state(d);
    int v = (int)gtk_range_get_value(r);
    if (v == st->mosaic_block) return;
    push_snapshot(st);
    st->mosaic_block = v;
    mo_refresh(st);
}
static void mo_on_pattern(GObject *dd, GParamSpec *p, gpointer d) {
    (void)p;
    GrainState *st = get_state(d);
    st->mosaic_pattern = gtk_drop_down_get_selected(GTK_DROP_DOWN(dd));
    mo_refresh(st);
}
static void mo_on_save(GtkButton *b, gpointer d) {
    (void)b; on_save_common(get_state(d), "mosaic");
}
static void mo_on_drop(const char *path, gpointer d) {
    GrainState *st = get_state(d);
    on_drop_common(st, path);
    if (st->original) mo_refresh(st);
}
static void mo_on_reset(GtkButton *b, gpointer d) {
    (void)b;
    GrainState *st = get_state(d);
    push_snapshot(st);
    st->mosaic_block = 12;
    st->mosaic_pattern = 0;
    mo_refresh(st);
}

void image_mosaic_on_close(GtkWidget *v) {
    g_object_set_data(G_OBJECT(v), "grain-state", NULL);
}
static void cmd_mo_reset(GtkWidget *v) { mo_on_reset(NULL, v); }

const HelvetiaToolCommand image_mosaic_commands[] = {
    { .id = "reset", .name = "Reset", .icon_name = "view-refresh-symbolic",
      .accel = NULL, .tooltip = "Reset", .activate = cmd_mo_reset },
    { NULL, NULL, NULL, NULL, NULL, NULL },
};

GtkWidget *image_mosaic_create(void) {
    GrainState *st = g_new0(GrainState, 1);
    st->undo_stack = g_ptr_array_new();
    st->zoom = 1.0;
    st->mosaic_block = 12;
    st->mosaic_pattern = 0;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(root, TRUE);
    st->root = root;

    GtkWidget *sliders;
    GtkWidget *stack = build_shell(st, &sliders, "Image file",
                                    mo_on_drop, G_CALLBACK(mo_on_save),
                                    G_CALLBACK(mo_on_reset), root);

    gtk_box_append(GTK_BOX(sliders),
        make_dropdown_row("Pattern",
                          (const char *[]){"Rectangular", "Hexagonal", "Brick", NULL},
                          0, G_CALLBACK(mo_on_pattern), root));

    GtkWidget *s, *l;
    gtk_box_append(GTK_BOX(sliders),
        make_slider("Block size", 4, 64, 1, 12, &s, &l,
                     G_CALLBACK(mo_on_block), root));
    gtk_label_set_text(GTK_LABEL(l), "12");

    gtk_box_append(GTK_BOX(root), stack);
    g_object_set_data_full(G_OBJECT(root), "grain-state", st,
                           (GDestroyNotify)grain_state_free);
    image_register_zoom(root, st->picture, &st->zoom);
    image_install_zoom_shortcuts(root);
    return root;
}
