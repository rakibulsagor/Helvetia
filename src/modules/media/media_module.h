#pragma once
#include "../../core/plugin.h"
#include <gtk/gtk.h>

const HelvetiaModule *helvetia_media_get_module(void);

GtkWidget *build_audio_converter  (void);
GtkWidget *build_video_converter  (void);
GtkWidget *build_media_trimmer    (void);
GtkWidget *build_video_to_gif     (void);
GtkWidget *build_video_metadata   (void);
