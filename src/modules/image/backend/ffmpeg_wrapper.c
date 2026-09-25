#include "ffmpeg_wrapper.h"
#include <glib.h>

static gboolean run(const char *const *argv, GError **error) {
    int status = 0;
    gchar *stderr_out = NULL;
    if (!g_spawn_sync(NULL, (gchar **)argv, NULL,
                      G_SPAWN_SEARCH_PATH, NULL, NULL,
                      NULL, &stderr_out, &status, error))
        return FALSE;
    if (status != 0) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                    "ffmpeg failed: %s", stderr_out ? stderr_out : "unknown");
        g_free(stderr_out);
        return FALSE;
    }
    g_free(stderr_out);
    return TRUE;
}

gboolean ffmpeg_convert(const char *in, const char *out, GError **error) {
    const char *argv[] = { "ffmpeg", "-y", "-i", in,
                           "-c", "copy", out, NULL };
    return run(argv, error);
}

gboolean ffmpeg_trim(const char *in, const char *out,
                     double start, double duration, GError **error) {
    char ss[32], t[32];
    g_snprintf(ss, sizeof ss, "%.3f", start);
    g_snprintf(t, sizeof t, "%.3f", duration);
    const char *argv[] = { "ffmpeg", "-y", "-ss", ss, "-i", in,
                           "-t", t, "-c", "copy", out, NULL };
    return run(argv, error);
}

gboolean ffmpeg_extract_audio(const char *in, const char *out,
                              const char *codec, GError **error) {
    const char *argv[] = { "ffmpeg", "-y", "-i", in,
                           "-vn", "-acodec", codec, out, NULL };
    return run(argv, error);
}

gboolean ffmpeg_to_gif(const char *in, const char *out,
                       double start, double duration,
                       int width, GError **error) {
    char ss[32], t[32], vf[128];
    g_snprintf(ss, sizeof ss, "%.3f", start);
    g_snprintf(t, sizeof t, "%.3f", duration);
    g_snprintf(vf, sizeof vf, "fps=12,scale=%d:-1:flags=lanczos", width);
    const char *argv[] = { "ffmpeg", "-y", "-ss", ss, "-i", in,
                           "-t", t, "-vf", vf, out, NULL };
    return run(argv, error);
}

gboolean ffmpeg_compress(const char *in, const char *out,
                         int crf, const char *preset, GError **error) {
    char crf_s[16];
    g_snprintf(crf_s, sizeof crf_s, "%d", crf);
    const char *argv[] = { "ffmpeg", "-y", "-i", in,
                           "-c:v", "libx264", "-crf", crf_s,
                           "-preset", preset, "-c:a", "aac",
                           "-b:a", "128k", out, NULL };
    return run(argv, error);
}
