#pragma once
#include <glib.h>

G_BEGIN_DECLS

gboolean tool_video_convert(const char *input, const char *output, GError **error);
gboolean tool_video_trim(const char *input, const char *output, double start, double dur, GError **error);
gboolean tool_video_extract_audio(const char *input, const char *output, GError **error);
gboolean tool_video_to_gif(const char *input, const char *output, double start, double dur, GError **error);

G_END_DECLS
