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
