#pragma once
#include "../../core/plugin.h"
#include <gtk/gtk.h>

const HelvetiaModule *helvetia_images_get_module(void);

/* ---- Viewing ---- */
GtkWidget *build_image_viewer          (void);
GtkWidget *build_image_slideshow       (void);
GtkWidget *build_image_thumbnail_grid  (void);
GtkWidget *build_exif_viewer           (void);
GtkWidget *build_metadata_inspector    (void);

/* ---- Basic Editing ---- */
GtkWidget *build_image_crop             (void);
GtkWidget *build_image_resize           (void);
GtkWidget *build_image_rotate           (void);
GtkWidget *build_image_flip             (void);
GtkWidget *build_image_straighten       (void);
GtkWidget *build_image_perspective_correct(void);
GtkWidget *build_image_canvas_resize    (void);
GtkWidget *build_image_auto_crop_borders(void);

/* ---- Adjustments ---- */
GtkWidget *build_image_brightness_contrast (void);
GtkWidget *build_image_levels              (void);
GtkWidget *build_image_curves              (void);
GtkWidget *build_image_exposure            (void);
GtkWidget *build_image_saturation_vibrance (void);
GtkWidget *build_image_hue_shift           (void);
GtkWidget *build_image_white_balance       (void);
GtkWidget *build_image_color_balance       (void);
GtkWidget *build_image_shadows_highlights  (void);
GtkWidget *build_image_gamma_correction    (void);
GtkWidget *build_image_auto_enhance        (void);
GtkWidget *build_image_histogram           (void);

/* ---- Filters ---- */
GtkWidget *build_image_blur             (void);
GtkWidget *build_image_sharpen          (void);
GtkWidget *build_image_unsharp_mask     (void);
GtkWidget *build_image_noise_reduction  (void);
GtkWidget *build_image_denoise          (void);
GtkWidget *build_image_sepia            (void);
GtkWidget *build_image_grayscale        (void);
GtkWidget *build_image_invert           (void);
GtkWidget *build_image_posterize        (void);
GtkWidget *build_image_threshold        (void);
GtkWidget *build_image_vignette         (void);
GtkWidget *build_image_film_grain       (void);
GtkWidget *build_image_glow             (void);
GtkWidget *build_image_emboss           (void);
GtkWidget *build_image_edge_detect      (void);
GtkWidget *build_image_pixelate         (void);
GtkWidget *build_image_mosaic           (void);

/* ---- Drawing & Compositing ---- */
GtkWidget *build_image_brush_tool       (void);
GtkWidget *build_image_eraser           (void);
GtkWidget *build_image_fill_bucket      (void);
GtkWidget *build_image_gradient_tool    (void);
GtkWidget *build_image_text_tool        (void);
GtkWidget *build_image_shape_tool       (void);
GtkWidget *build_image_arrow_tool       (void);
GtkWidget *build_image_eyedropper       (void);
GtkWidget *build_image_clone_stamp      (void);
GtkWidget *build_image_healing_brush    (void);
GtkWidget *build_image_smudge_tool      (void);
GtkWidget *build_image_dodge_burn       (void);
GtkWidget *build_image_selection_tools  (void);
GtkWidget *build_image_layer_manager    (void);
GtkWidget *build_image_layer_masks      (void);
GtkWidget *build_image_blend_modes      (void);
GtkWidget *build_image_opacity_control  (void);

/* ---- Transform ---- */
GtkWidget *build_image_scale            (void);
GtkWidget *build_image_skew             (void);
GtkWidget *build_image_distort          (void);
GtkWidget *build_image_warp             (void);
GtkWidget *build_image_free_transform   (void);

/* ---- Retouching ---- */
GtkWidget *build_image_background_removal  (void);
GtkWidget *build_image_background_replace  (void);
GtkWidget *build_image_portrait_retouch    (void);
GtkWidget *build_image_blemish_removal     (void);
GtkWidget *build_image_red_eye_removal     (void);
GtkWidget *build_image_teeth_whitening     (void);
GtkWidget *build_image_skin_smoothing      (void);

/* ---- Composition ---- */
GtkWidget *build_image_collage_maker    (void);
GtkWidget *build_image_grid_layout      (void);
GtkWidget *build_image_panorama_stitch  (void);
GtkWidget *build_image_hdr_merge        (void);
GtkWidget *build_image_focus_stack      (void);
GtkWidget *build_image_image_stack      (void);
GtkWidget *build_image_watermark        (void);
GtkWidget *build_image_border_frame     (void);
GtkWidget *build_image_drop_shadow      (void);
GtkWidget *build_image_reflection       (void);

/* ---- Format & Conversion ---- */
GtkWidget *build_image_converter        (void);
GtkWidget *build_image_batch_convert    (void);
GtkWidget *build_image_heic_to_jpg      (void);
GtkWidget *build_image_raw_to_jpg       (void);
GtkWidget *build_image_svg_to_png       (void);
GtkWidget *build_image_png_to_svg       (void);
GtkWidget *build_image_to_base64        (void);
GtkWidget *build_image_compression      (void);
GtkWidget *build_image_lossless_optimizer(void);

/* ---- Special ---- */
GtkWidget *build_image_qr_reader            (void);
GtkWidget *build_image_barcode_reader       (void);
GtkWidget *build_image_steganography        (void);
GtkWidget *build_image_to_ascii_art         (void);
GtkWidget *build_image_pixel_art_scaler     (void);
GtkWidget *build_image_sprite_sheet_slicer  (void);
GtkWidget *build_image_animated_gif_maker   (void);
GtkWidget *build_image_animated_gif_splitter(void);
GtkWidget *build_image_screenshot_tool      (void);
GtkWidget *build_image_screen_recorder      (void);
