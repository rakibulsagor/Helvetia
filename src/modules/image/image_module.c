#include "../../core/plugin.h"
#include "tools/image_viewer.h"
#include "tools/image_slideshow.h"
#include "tools/image_thumbnail_grid.h"
#include "tools/image_exif_viewer.h"
#include <stddef.h>

static const HelvetiaTool tool_image_viewer = {
    .id = "image_viewer",
    .name = "Image Viewer",
    .description = "View images with zoom, pan, and rotate",
    .icon_name = "image-x-generic-symbolic",
    .keywords = (const char*[]){ "image", "viewer", "open", "zoom", "pan", "rotate", NULL },
    .create_view = image_viewer_create,
    .commands = image_viewer_commands,
    .on_close = image_viewer_on_close,
};

static const HelvetiaTool tool_image_slideshow = {
    .id = "image_slideshow",
    .name = "Image Slideshow",
    .description = "Present images full-screen with auto-advance",
    .icon_name = "media-playback-start-symbolic",
    .keywords = (const char*[]){ "slideshow", "presentation", "play", "auto", "advance", NULL },
    .create_view = image_slideshow_create,
    .commands = image_slideshow_commands,
    .on_close = image_slideshow_on_close,
};

static const HelvetiaTool tool_image_thumbnail_grid = {
    .id = "image_thumbnail_grid",
    .name = "Image Thumbnail Grid",
    .description = "Browse a folder as a grid of thumbnails",
    .icon_name = "view-grid-symbolic",
    .keywords = (const char*[]){ "thumbnail", "grid", "browse", "contact", "sheet", NULL },
    .create_view = image_thumbnail_grid_create,
    .commands = image_thumbnail_grid_commands,
    .on_close = image_thumbnail_grid_on_close,
};

static const HelvetiaTool tool_image_exif_viewer = {
    .id = "image_exif_viewer",
    .name = "EXIF Viewer",
    .description = "Inspect camera, lens, and GPS metadata",
    .icon_name = "camera-photo-symbolic",
    .keywords = (const char*[]){ "exif", "metadata", "camera", "lens", "gps", "aperture", "iso", NULL },
    .create_view = image_exif_viewer_create,
    .commands = image_exif_viewer_commands,
    .on_close = image_exif_viewer_on_close,
};

static const HelvetiaTool category_viewing[] = {
    { .id = "image_viewer", .name = "Image Viewer", .description = "View images with zoom, pan, and rotate", .icon_name = "image-x-generic-symbolic", .keywords = (const char*[]){ "image", "viewer", "open", "zoom", "pan", "rotate", NULL }, .create_view = image_viewer_create, .commands = image_viewer_commands, .on_close = image_viewer_on_close },
    { .id = "image_slideshow", .name = "Image Slideshow", .description = "Present images full-screen with auto-advance", .icon_name = "media-playback-start-symbolic", .keywords = (const char*[]){ "slideshow", "presentation", "play", "auto", "advance", NULL }, .create_view = image_slideshow_create, .commands = image_slideshow_commands, .on_close = image_slideshow_on_close },
    { .id = "image_thumbnail_grid", .name = "Image Thumbnail Grid", .description = "Browse a folder as a grid of thumbnails", .icon_name = "view-grid-symbolic", .keywords = (const char*[]){ "thumbnail", "grid", "browse", "contact", "sheet", NULL }, .create_view = image_thumbnail_grid_create, .commands = image_thumbnail_grid_commands, .on_close = image_thumbnail_grid_on_close },
    { .id = "image_exif_viewer", .name = "EXIF Viewer", .description = "Inspect camera, lens, and GPS metadata", .icon_name = "camera-photo-symbolic", .keywords = (const char*[]){ "exif", "metadata", "camera", "lens", "gps", "aperture", "iso", NULL }, .create_view = image_exif_viewer_create, .commands = image_exif_viewer_commands, .on_close = image_exif_viewer_on_close },
    { NULL }
};

static const HelvetiaSubcategory image_subcategories[] = {
    {
        .name = "Viewing & Inspection",
        .tools = category_viewing,
    },
    { NULL }
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
