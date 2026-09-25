#include "image_video_convert.h"
#include "../backend/ffmpeg_wrapper.h"

gboolean tool_video_convert(const char *input, const char *output, GError **error) {
    return ffmpeg_convert(input, output, error);
}

gboolean tool_video_trim(const char *input, const char *output, double start, double dur, GError **error) {
    return ffmpeg_trim(input, output, start, dur, error);
}

gboolean tool_video_extract_audio(const char *input, const char *output, GError **error) {
    return ffmpeg_extract_audio(input, output, "libmp3lame", error);
}

gboolean tool_video_to_gif(const char *input, const char *output, double start, double dur, GError **error) {
    return ffmpeg_to_gif(input, output, start, dur, 480, error);
}
