#pragma once
#include <glib.h>

G_BEGIN_DECLS

gboolean ffmpeg_convert(const char *in, const char *out, GError **error);
gboolean ffmpeg_trim(const char *in, const char *out,
                     double start_sec, double duration_sec, GError **error);
gboolean ffmpeg_extract_audio(const char *in, const char *out,
                              const char *codec, GError **error);
gboolean ffmpeg_to_gif(const char *in, const char *out,
                       double start, double duration, int width, GError **error);
gboolean ffmpeg_compress(const char *in, const char *out,
                         int crf, const char *preset, GError **error);

G_END_DECLS
