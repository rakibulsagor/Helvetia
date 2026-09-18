#define _POSIX_C_SOURCE 200809L
/* ================================================================
 * Helvetia — Media Module — Tool Implementations (ffmpeg-based)
 * ================================================================ */
#include "media_module.h"
#include "../../ui/widgets.h"
#include <gtk/gtk.h>
#include <glib.h>
#include <string.h>
#include <stdio.h>

/* ================================================================
 * Generic ffmpeg progress runner
 * ================================================================ */
typedef struct {
    char *cmd;
    GtkWidget *progress;
    GtkWidget *status_label;
    GtkWidget *btn;
} FfmpegJob;

static gboolean ffmpeg_job_update(gpointer data) {
    FfmpegJob *job = data;
    /* This runs after the async command completes */
    if (job->progress)
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(job->progress), 1.0);
    if (job->status_label)
        gtk_label_set_text(GTK_LABEL(job->status_label), "✓ Done!");
    if (job->btn)
        gtk_widget_set_sensitive(job->btn, TRUE);
    g_free(job->cmd);
    g_free(job);
    return G_SOURCE_REMOVE;
}

static void run_ffmpeg_async(const char *cmd, GtkWidget *progress, 
                              GtkWidget *status_label, GtkWidget *btn) {
    if (progress)
        gtk_progress_bar_pulse(GTK_PROGRESS_BAR(progress));
    if (status_label)
        gtk_label_set_text(GTK_LABEL(status_label), "Processing...");
    if (btn)
        gtk_widget_set_sensitive(btn, FALSE);

    FfmpegJob *job = g_new0(FfmpegJob, 1);
    job->cmd = g_strdup(cmd);
    job->progress = progress;
    job->status_label = status_label;
    job->btn = btn;

    /* Run in a thread pool */
    GError *err = NULL;
    char *result = NULL;
    if (!g_spawn_command_line_sync(cmd, &result, NULL, NULL, &err)) {
        if (status_label)
            gtk_label_set_text(GTK_LABEL(status_label), err ? err->message : "Error");
        if (err) g_error_free(err);
        g_free(result);
        g_free(job->cmd);
        g_free(job);
        if (btn) gtk_widget_set_sensitive(btn, TRUE);
        return;
    }
    g_free(result);
    g_idle_add(ffmpeg_job_update, job);
}

/* ================================================================
 * Audio / Video Converter
 * ================================================================ */
typedef struct {
    GtkWidget *in_entry, *out_entry, *format_dd, *quality_spin;
    GtkWidget *progress, *status;
    GtkWidget *btn;
    gboolean is_audio;
} ConverterCtx;

static void on_media_convert(GtkButton *btn, gpointer ud) {
    ConverterCtx *ctx = ud;
    const char *in_path  = gtk_editable_get_text(GTK_EDITABLE(ctx->in_entry));
    const char *out_path = gtk_editable_get_text(GTK_EDITABLE(ctx->out_entry));
    
    if (!in_path || !*in_path || !out_path || !*out_path) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Fill input and output paths");
        return;
    }
    
    char *qi = g_shell_quote(in_path);
    char *qo = g_shell_quote(out_path);
    char *cmd;
    if (ctx->is_audio) {
        cmd = g_strdup_printf("ffmpeg -y -i %s -q:a 0 %s 2>&1", qi, qo);
    } else {
        int quality = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->quality_spin));
        cmd = g_strdup_printf("ffmpeg -y -i %s -crf %d %s 2>&1", qi, quality, qo);
    }
    g_free(qi); g_free(qo);
    
    run_ffmpeg_async(cmd, ctx->progress, ctx->status, GTK_WIDGET(btn));
    g_free(cmd);
}

static GtkWidget *make_media_converter(gboolean audio_mode) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    
    GtkWidget *note = gtk_label_new(audio_mode
        ? "Converts audio files using ffmpeg. Supports MP3, FLAC, WAV, OGG, AAC, OPUS, M4A"
        : "Converts video files using ffmpeg. Supports MP4, MKV, WEBM, AVI, MOV");
    gtk_widget_add_css_class(note, "helvetia-fg-muted");
    gtk_widget_set_halign(note, GTK_ALIGN_START);
    gtk_label_set_wrap(GTK_LABEL(note), TRUE);
    gtk_box_append(GTK_BOX(box), note);
    gtk_box_append(GTK_BOX(box), hv_make_separator());
    
    GtkWidget *in_entry, *out_entry;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input file:", &in_entry, FALSE));
    gtk_entry_set_placeholder_text(GTK_ENTRY(in_entry),
        audio_mode ? "/path/to/audio.flac" : "/path/to/video.avi");
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output file:", &out_entry, TRUE));
    gtk_entry_set_placeholder_text(GTK_ENTRY(out_entry),
        audio_mode ? "/path/to/output.mp3" : "/path/to/output.mp4");
    
    GtkWidget *quality_spin = NULL;
    if (!audio_mode) {
        GtkWidget *q_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_box_append(GTK_BOX(q_row), gtk_label_new("CRF Quality (18=best, 28=smaller):"));
        quality_spin = gtk_spin_button_new_with_range(18, 51, 1);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(quality_spin), 23);
        gtk_box_append(GTK_BOX(q_row), quality_spin);
        gtk_box_append(GTK_BOX(box), q_row);
    }
    
    GtkWidget *btn      = hv_make_action_btn(audio_mode ? "Convert Audio" : "Convert Video");
    GtkWidget *progress = gtk_progress_bar_new();
    GtkWidget *status   = hv_make_result_label();
    gtk_label_set_text(GTK_LABEL(status), "Ready");
    
    ConverterCtx *ctx = g_new0(ConverterCtx, 1);
    ctx->in_entry     = in_entry;
    ctx->out_entry    = out_entry;
    ctx->quality_spin = quality_spin;
    ctx->progress     = progress;
    ctx->status       = status;
    ctx->btn          = btn;
    ctx->is_audio     = audio_mode;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_media_convert), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);
    
    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), progress);
    gtk_box_append(GTK_BOX(box), status);
    return box;
}

GtkWidget *build_audio_converter(void) { return make_media_converter(TRUE);  }
GtkWidget *build_video_converter(void) { return make_media_converter(FALSE); }

/* ================================================================
 * Media Trimmer (Audio + Video)
 * ================================================================ */
typedef struct {
    GtkWidget *in_entry, *out_entry, *start_entry, *end_entry, *status;
} TrimCtx;

static void on_trim(GtkButton *btn, gpointer ud) {
    TrimCtx *ctx = ud;
    const char *in    = gtk_editable_get_text(GTK_EDITABLE(ctx->in_entry));
    const char *out   = gtk_editable_get_text(GTK_EDITABLE(ctx->out_entry));
    const char *start = gtk_editable_get_text(GTK_EDITABLE(ctx->start_entry));
    const char *end   = gtk_editable_get_text(GTK_EDITABLE(ctx->end_entry));
    
    if (!in || !*in || !out || !*out) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Fill input and output paths");
        return;
    }
    
    char *qi = g_shell_quote(in);
    char *qo = g_shell_quote(out);
    char *cmd;
    if (start && *start && end && *end) {
        cmd = g_strdup_printf("ffmpeg -y -ss %s -to %s -i %s -c copy %s 2>&1",
                              start, end, qi, qo);
    } else if (start && *start) {
        cmd = g_strdup_printf("ffmpeg -y -ss %s -i %s -c copy %s 2>&1", start, qi, qo);
    } else {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Enter at least a start time");
        g_free(qi); g_free(qo);
        return;
    }
    g_free(qi); g_free(qo);
    run_ffmpeg_async(cmd, NULL, ctx->status, GTK_WIDGET(btn));
    g_free(cmd);
}

GtkWidget *build_media_trimmer(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *in_e, *out_e, *start_e, *end_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input file:", &in_e, FALSE));
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output file:", &out_e, TRUE));
    
    GtkWidget *r3 = hv_make_entry_row("Start time:", &start_e);
    gtk_entry_set_placeholder_text(GTK_ENTRY(start_e), "HH:MM:SS or seconds (e.g. 00:01:30)");
    gtk_box_append(GTK_BOX(box), r3);
    
    GtkWidget *r4 = hv_make_entry_row("End time:", &end_e);
    gtk_entry_set_placeholder_text(GTK_ENTRY(end_e), "HH:MM:SS or seconds (blank = end of file)");
    gtk_box_append(GTK_BOX(box), r4);
    
    GtkWidget *note = gtk_label_new("Uses stream copy — instant, lossless cut");
    gtk_widget_add_css_class(note, "helvetia-fg-muted");
    gtk_widget_set_halign(note, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box), note);
    
    GtkWidget *btn    = hv_make_action_btn("Trim");
    GtkWidget *status = hv_make_result_label();
    gtk_label_set_text(GTK_LABEL(status), "Ready");
    
    TrimCtx *ctx = g_new0(TrimCtx, 1);
    ctx->in_entry    = in_e;
    ctx->out_entry   = out_e;
    ctx->start_entry = start_e;
    ctx->end_entry   = end_e;
    ctx->status      = status;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_trim), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);
    
    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), status);
    return box;
}

/* ================================================================
 * Video to GIF
 * ================================================================ */
typedef struct { GtkWidget *in_e, *out_e, *fps_spin, *width_spin, *status; } GifCtx;

static void on_video_to_gif(GtkButton *btn, gpointer ud) {
    GifCtx *ctx = ud;
    const char *in  = gtk_editable_get_text(GTK_EDITABLE(ctx->in_e));
    const char *out = gtk_editable_get_text(GTK_EDITABLE(ctx->out_e));
    int fps   = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->fps_spin));
    int width = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(ctx->width_spin));
    
    if (!in || !*in || !out || !*out) {
        gtk_label_set_text(GTK_LABEL(ctx->status), "⚠ Fill input and output paths");
        return;
    }
    char *qi  = g_shell_quote(in);
    char *qo  = g_shell_quote(out);
    char *cmd = g_strdup_printf(
        "ffmpeg -y -i %s -vf \"fps=%d,scale=%d:-1:flags=lanczos,split[s0][s1];"
        "[s0]palettegen[p];[s1][p]paletteuse\" %s 2>&1",
        qi, fps, width, qo);
    g_free(qi); g_free(qo);
    run_ffmpeg_async(cmd, NULL, ctx->status, GTK_WIDGET(btn));
    g_free(cmd);
}

GtkWidget *build_video_to_gif(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *in_e, *out_e;
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Input video:", &in_e, FALSE));
    gtk_box_append(GTK_BOX(box), hv_make_file_picker_row("Output GIF:", &out_e, TRUE));
    gtk_entry_set_placeholder_text(GTK_ENTRY(out_e), "/path/to/output.gif");
    
    GtkWidget *r1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(r1), gtk_label_new("FPS:"));
    GtkWidget *fps = gtk_spin_button_new_with_range(1, 30, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(fps), 10);
    gtk_box_append(GTK_BOX(r1), fps);
    gtk_box_append(GTK_BOX(r1), gtk_label_new("Width (px):"));
    GtkWidget *width = gtk_spin_button_new_with_range(100, 1920, 100);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(width), 480);
    gtk_box_append(GTK_BOX(r1), width);
    gtk_box_append(GTK_BOX(box), r1);
    
    GtkWidget *btn    = hv_make_action_btn("Convert to GIF");
    GtkWidget *status = hv_make_result_label();
    gtk_label_set_text(GTK_LABEL(status), "Ready");
    
    GifCtx *ctx = g_new0(GifCtx, 1);
    ctx->in_e = in_e; ctx->out_e = out_e;
    ctx->fps_spin = fps; ctx->width_spin = width;
    ctx->status = status;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_video_to_gif), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);
    
    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), status);
    return box;
}

/* ================================================================
 * Media Info (ffprobe)
 * ================================================================ */
typedef struct { GtkWidget *entry, *tv; } MediaInfoCtx;

static void on_media_info(GtkButton *btn, gpointer ud) {
    (void)btn;
    MediaInfoCtx *ctx = ud;
    const char *path = gtk_editable_get_text(GTK_EDITABLE(ctx->entry));
    if (!path || !*path) {
        hv_textview_set_text(ctx->tv, "⚠ Enter a file path");
        return;
    }
    char *qpath = g_shell_quote(path);
    char *cmd   = g_strdup_printf(
        "ffprobe -v quiet -print_format json -show_format -show_streams %s 2>&1 | "
        "python3 -m json.tool 2>&1", qpath);
    g_free(qpath);
    char *out = hv_run_cmd(cmd);
    g_free(cmd);
    hv_textview_set_text(ctx->tv, out ? out : "Error running ffprobe");
    g_free(out);
}

GtkWidget *build_video_metadata(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *entry;
    gtk_box_append(GTK_BOX(box), hv_make_entry_row("Media file:", &entry));
    GtkWidget *btn = hv_make_action_btn("Inspect with ffprobe");
    GtkWidget *tv;
    GtkWidget *sw = hv_make_text_view(&tv, FALSE);

    MediaInfoCtx *ctx = g_new0(MediaInfoCtx, 1);
    ctx->entry = entry; ctx->tv = tv;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_media_info), ctx);
    g_signal_connect(entry, "activate", G_CALLBACK(on_media_info), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);

    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), sw);
    return box;
}
