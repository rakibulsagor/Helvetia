#include "../../core/plugin.h"
#include "tools/image_viewer.h"
#include "tools/image_slideshow.h"
#include "tools/image_thumbnail_grid.h"
#include "tools/image_exif_viewer.h"
#include "tools/image_metadata_inspector.h"
#include "tools/image_crop.h"
#include "tools/image_geometry.h"
#include "tools/image_autocrop.h"
#include "tools/image_perspective.h"
#include "tools/image_brightness_contrast.h"
#include "tools/image_levels.h"
#include "tools/image_curves.h"
#include "tools/image_exposure.h"
#include "tools/image_color.h"
#include "tools/image_tone.h"
#include <stddef.h>

static const HelvetiaTool category_viewing[] = {
    { .id = "image_viewer", .name = "Image Viewer", .description = "View images with zoom, pan, and rotate", .icon_name = "image-x-generic-symbolic", .keywords = (const char*[]){ "image", "viewer", "open", "zoom", "pan", "rotate", NULL }, .create_view = image_viewer_create, .commands = image_viewer_commands, .on_close = image_viewer_on_close },
    { .id = "image_slideshow", .name = "Image Slideshow", .description = "Present images full-screen with auto-advance", .icon_name = "media-playback-start-symbolic", .keywords = (const char*[]){ "slideshow", "presentation", "play", "auto", "advance", NULL }, .create_view = image_slideshow_create, .commands = image_slideshow_commands, .on_close = image_slideshow_on_close },
    { .id = "image_thumbnail_grid", .name = "Image Thumbnail Grid", .description = "Browse a folder as a grid of thumbnails", .icon_name = "view-grid-symbolic", .keywords = (const char*[]){ "thumbnail", "grid", "browse", "contact", "sheet", NULL }, .create_view = image_thumbnail_grid_create, .commands = image_thumbnail_grid_commands, .on_close = image_thumbnail_grid_on_close },
    { .id = "image_exif_viewer", .name = "EXIF Viewer", .description = "Inspect camera, lens, and GPS metadata", .icon_name = "camera-photo-symbolic", .keywords = (const char*[]){ "exif", "metadata", "camera", "lens", "gps", "aperture", "iso", NULL }, .create_view = image_exif_viewer_create, .commands = image_exif_viewer_commands, .on_close = image_exif_viewer_on_close },
    { .id = "image_metadata_inspector", .name = "Metadata Inspector", .description = "Show all metadata: file, image, and EXIF", .icon_name = "document-properties-symbolic", .keywords = (const char*[]){ "metadata", "inspector", "file", "exif", "properties", "info", NULL }, .create_view = image_metadata_inspector_create, .commands = image_metadata_inspector_commands, .on_close = image_metadata_inspector_on_close },
    { 0 }
};

static const HelvetiaTool category_basic[] = {
    { .id = "image_crop", .name = "Crop", .description = "Trim to a selection", .icon_name = "transform-crop-symbolic", .keywords = (const char*[]){ "crop", "trim", "cut", "selection", NULL }, .create_view = image_crop_create, .commands = image_crop_commands, .on_close = image_crop_on_close },
    { .id = "image_resize", .name = "Resize", .description = "Scale to new dimensions", .icon_name = "transform-scale-symbolic", .keywords = (const char*[]){ "resize", "scale", "dimensions", "width", "height", NULL }, .create_view = image_resize_create, .on_close = image_resize_on_close },
    { .id = "image_rotate", .name = "Rotate", .description = "Rotate 90°, 180°, 270°", .icon_name = "object-rotate-right-symbolic", .keywords = (const char*[]){ "rotate", "turn", "angle", "90", "180", "270", NULL }, .create_view = image_rotate_create, .on_close = image_rotate_on_close },
    { .id = "image_flip", .name = "Flip", .description = "Mirror horizontally or vertically", .icon_name = "object-flip-horizontal-symbolic", .keywords = (const char*[]){ "flip", "mirror", "horizontal", "vertical", NULL }, .create_view = image_flip_create, .on_close = image_flip_on_close },
    { .id = "image_straighten", .name = "Straighten", .description = "Correct a tilted horizon", .icon_name = "object-rotate-left-symbolic", .keywords = (const char*[]){ "straighten", "tilt", "horizon", "angle", "rotate", NULL }, .create_view = image_straighten_create, .on_close = image_straighten_on_close },
    { .id = "image_perspective", .name = "Perspective Correct", .description = "Fix keystone distortion", .icon_name = "transform-skew-symbolic", .keywords = (const char*[]){ "perspective", "correct", "keystone", "distortion", NULL }, .create_view = image_perspective_create, .on_close = image_perspective_on_close },
    { .id = "image_canvas_resize", .name = "Canvas Resize", .description = "Change canvas without scaling", .icon_name = "document-page-setup-symbolic", .keywords = (const char*[]){ "canvas", "resize", "crop", "pad", NULL }, .create_view = image_canvas_resize_create, .on_close = image_canvas_resize_on_close },
    { .id = "image_autocrop", .name = "Auto-crop Borders", .description = "Trim uniform border color", .icon_name = "transform-crop-and-resize-symbolic", .keywords = (const char*[]){ "autocrop", "borders", "trim", "auto", "border", NULL }, .create_view = image_autocrop_create, .on_close = image_autocrop_on_close },
    { 0 }
};

static const HelvetiaTool category_adjustments[] = {
    { .id = "image_brightness_contrast", .name = "Brightness / Contrast", .description = "Adjust overall lightness and tonal range", .icon_name = "display-brightness-symbolic", .keywords = (const char*[]){ "brightness", "contrast", "lighten", "darken", "adjust", NULL }, .create_view = image_brightness_contrast_create, .commands = image_brightness_contrast_commands, .on_close = image_brightness_contrast_on_close },
    { .id = "image_levels", .name = "Levels", .description = "Adjust black point, white point, and gamma", .icon_name = "preferences-color-symbolic", .keywords = (const char*[]){ "levels", "histogram", "black", "white", "gamma", "tone", NULL }, .create_view = image_levels_create, .commands = image_levels_commands, .on_close = image_levels_on_close },
    { .id = "image_curves", .name = "Curves", .description = "Adjust tone curves with control points", .icon_name = "preferences-color-symbolic", .keywords = (const char*[]){ "curves", "tone", "gamma", "contrast", "rgb", "channel", "spline", NULL }, .create_view = image_curves_create, .commands = image_curves_commands, .on_close = image_curves_on_close },
    { .id = "image_exposure", .name = "Exposure", .description = "Adjust EV and black point", .icon_name = "display-brightness-symbolic", .keywords = (const char*[]){ "exposure", "ev", "stops", "black", "point", "brightness", NULL }, .create_view = image_exposure_create, .commands = image_exposure_commands, .on_close = image_exposure_on_close },
    { .id = "image_saturation_vibrance", .name = "Saturation / Vibrance", .description = "Adjust color intensity", .icon_name = "preferences-color-symbolic", .keywords = (const char*[]){ "saturation", "vibrance", "color", "intensity", "vivid", NULL }, .create_view = image_saturation_vibrance_create, .commands = image_saturation_vibrance_commands, .on_close = image_saturation_vibrance_on_close },
    { .id = "image_hue_shift", .name = "Hue Shift", .description = "Rotate all hues in the image", .icon_name = "preferences-color-symbolic", .keywords = (const char*[]){ "hue", "shift", "rotate", "color", "wheel", "hsl", NULL }, .create_view = image_hue_shift_create, .commands = image_hue_shift_commands, .on_close = image_hue_shift_on_close },
    { .id = "image_white_balance", .name = "White Balance", .description = "Correct color temperature and tint", .icon_name = "preferences-color-symbolic", .keywords = (const char*[]){ "white", "balance", "temperature", "tint", "warm", "cool", NULL }, .create_view = image_white_balance_create, .commands = image_white_balance_commands, .on_close = image_white_balance_on_close },
    { .id = "image_color_balance", .name = "Color Balance", .description = "Adjust RGB per tonal range", .icon_name = "preferences-color-symbolic", .keywords = (const char*[]){ "color", "balance", "shadows", "mids", "highlights", "rgb", NULL }, .create_view = image_color_balance_create, .commands = image_color_balance_commands, .on_close = image_color_balance_on_close },
    { .id = "image_shadows_highlights", .name = "Shadows / Highlights", .description = "Recover detail in shadows and highlights", .icon_name = "preferences-color-symbolic", .keywords = (const char*[]){ "shadows", "highlights", "recovery", "tone", "range", NULL }, .create_view = image_shadows_highlights_create, .commands = image_shadows_highlights_commands, .on_close = image_shadows_highlights_on_close },
    { .id = "image_gamma", .name = "Gamma Correction", .description = "Adjust midtone brightness curve", .icon_name = "preferences-color-symbolic", .keywords = (const char*[]){ "gamma", "correction", "midtone", "curve", NULL }, .create_view = image_gamma_create, .commands = image_gamma_commands, .on_close = image_gamma_on_close },
    { .id = "image_auto_enhance", .name = "Auto Enhance", .description = "One-click improvement", .icon_name = "starred-symbolic", .keywords = (const char*[]){ "auto", "enhance", "improve", "one", "click", "smart", NULL }, .create_view = image_auto_enhance_create, .commands = image_auto_enhance_commands, .on_close = image_auto_enhance_on_close },
    { .id = "image_histogram", .name = "Histogram", .description = "View RGB and luminance distribution", .icon_name = "utilities-system-monitor-symbolic", .keywords = (const char*[]){ "histogram", "chart", "distribution", "rgb", "luma", "statistics", NULL }, .create_view = image_histogram_create, .commands = image_histogram_commands, .on_close = image_histogram_on_close },
    { 0 }
};

static const HelvetiaSubcategory image_subcategories[] = {
    {
        .name = "Viewing & Inspection",
        .tools = category_viewing,
    },
    {
        .name = "Basic Editing",
        .tools = category_basic,
    },
    {
        .name = "Adjustments",
        .tools = category_adjustments,
    },
    { 0 }
};

static const HelvetiaModule image_module = {
    .id = "image",
    .name = "Images",
    .icon_name = "image-x-generic-symbolic",
    .description = "View, edit, and convert images.",
    .subcategories = image_subcategories,
    .create_view = NULL,
};

const HelvetiaModule *helvetia_image_get_module(void) {
    return &image_module;
}
