/* ================================================================
 * Helvetia — Images Module
 * ================================================================ */
#include "images_module.h"
#include <stddef.h>

static const HelvetiaTool tools_viewing[] = {
    { "image_viewer",        "Image Viewer",        "Image Viewer",        "eog-symbolic",             (const char*[]){ "image", "viewer", "images", NULL },               "image-viewer",     build_image_viewer         },
    { "image_slideshow",     "Image Slideshow",     "Image Slideshow",     "image-x-generic-symbolic", (const char*[]){ "image", "images", "slideshow", NULL },             "image-slideshow",  build_image_slideshow      },
    { "image_thumbnail_grid","Image Thumbnail Grid","Image Thumbnail Grid","image-x-generic-symbolic", (const char*[]){ "thumbnail", "image", "images", "grid", NULL },     "image-thumbnail-grid", build_image_thumbnail_grid },
    { "exif_viewer",         "EXIF Viewer",         "EXIF Viewer",         "eog-symbolic",             (const char*[]){ "exif", "viewer", "images", NULL },                 "exif-viewer",      build_exif_viewer          },
    { "metadata_inspector",  "Metadata Inspector",  "Metadata Inspector",  "image-x-generic-symbolic", (const char*[]){ "inspector", "metadata", "images", NULL },          "metadata-inspector", build_metadata_inspector  },
    { NULL }
};

static const HelvetiaTool tools_basic_editing[] = {
    { "crop",               "Crop",             "Crop",             "crop-symbolic",            (const char*[]){ "crop", "images", NULL },                           "crop",             build_image_crop              },
    { "resize",             "Resize",           "Resize",           "image-x-generic-symbolic", (const char*[]){ "resize", "images", NULL },                        "resize",           build_image_resize            },
    { "rotate",             "Rotate",           "Rotate",           "image-x-generic-symbolic", (const char*[]){ "rotate", "images", NULL },                        "rotate",           build_image_rotate            },
    { "flip",               "Flip",             "Flip",             "image-x-generic-symbolic", (const char*[]){ "flip", "images", NULL },                          "flip",             build_image_flip              },
    { "straighten",         "Straighten",       "Straighten",       "image-x-generic-symbolic", (const char*[]){ "images", "straighten", NULL },                    "straighten",       build_image_straighten        },
    { "perspective_correct","Perspective Correct","Perspective Correct","image-x-generic-symbolic",(const char*[]){ "perspective", "images", "correct", NULL },      "perspective-correct", build_image_perspective_correct },
    { "canvas_resize",      "Canvas Resize",    "Canvas Resize",    "image-x-generic-symbolic", (const char*[]){ "canvas", "resize", "images", NULL },              "canvas-resize",    build_image_canvas_resize     },
    { "auto_crop_borders",  "Auto-crop Borders","Auto-crop Borders","crop-symbolic",            (const char*[]){ "autocrop", "borders", "images", NULL },            "auto-crop-borders", build_image_auto_crop_borders },
    { NULL }
};

static const HelvetiaTool tools_adjustments[] = {
    { "brightness_contrast","Brightness / Contrast","Brightness / Contrast","color-select-symbolic",(const char*[]){ "brightness", "contrast", "images", NULL },    "brightness-contrast", build_image_brightness_contrast },
    { "levels",             "Levels",           "Levels",           "image-x-generic-symbolic", (const char*[]){ "images", "levels", NULL },                        "levels",           build_image_levels            },
    { "curves",             "Curves",           "Curves",           "image-x-generic-symbolic", (const char*[]){ "curves", "images", NULL },                        "curves",           build_image_curves            },
    { "exposure",           "Exposure",         "Exposure",         "image-x-generic-symbolic", (const char*[]){ "exposure", "images", NULL },                      "exposure",         build_image_exposure          },
    { "saturation_vibrance","Saturation / Vibrance","Saturation / Vibrance","image-x-generic-symbolic",(const char*[]){ "vibrance", "images", "saturation", NULL }, "saturation-vibrance", build_image_saturation_vibrance },
    { "hue_shift",          "Hue Shift",        "Hue Shift",        "color-select-symbolic",    (const char*[]){ "images", "hue", "shift", NULL },                  "hue-shift",        build_image_hue_shift         },
    { "white_balance",      "White Balance",    "White Balance",    "image-x-generic-symbolic", (const char*[]){ "images", "white", "balance", NULL },              "white-balance",    build_image_white_balance     },
    { "color_balance",      "Color Balance",    "Color Balance",    "color-select-symbolic",    (const char*[]){ "color", "images", "balance", NULL },              "color-balance",    build_image_color_balance     },
    { "shadows_highlights", "Shadows / Highlights","Shadows / Highlights","image-x-generic-symbolic",(const char*[]){ "shadows", "highlights", "images", NULL },   "shadows-highlights", build_image_shadows_highlights },
    { "gamma_correction",   "Gamma Correction", "Gamma Correction", "image-x-generic-symbolic", (const char*[]){ "correction", "images", "gamma", NULL },           "gamma-correction", build_image_gamma_correction  },
    { "auto_enhance",       "Auto Enhance",     "Auto Enhance",     "image-x-generic-symbolic", (const char*[]){ "images", "auto", "enhance", NULL },               "auto-enhance",     build_image_auto_enhance      },
    { "histogram",          "Histogram",        "Histogram",        "image-x-generic-symbolic", (const char*[]){ "histogram", "images", NULL },                     "histogram",        build_image_histogram         },
    { NULL }
};

static const HelvetiaTool tools_filters[] = {
    { "blur",           "Blur",           "Gaussian, motion, radial", "image-x-generic-symbolic", (const char*[]){ "blur", "images", NULL },           "blur",         build_image_blur          },
    { "sharpen",        "Sharpen",        "Sharpen",                  "image-x-generic-symbolic", (const char*[]){ "sharpen", "images", NULL },         "sharpen",      build_image_sharpen       },
    { "unsharp_mask",   "Unsharp Mask",   "Unsharp Mask",             "image-x-generic-symbolic", (const char*[]){ "images", "mask", "unsharp", NULL }, "unsharp-mask", build_image_unsharp_mask  },
    { "noise_reduction","Noise Reduction","Noise Reduction",          "image-x-generic-symbolic", (const char*[]){ "noise", "images", "reduction", NULL },"noise-reduction", build_image_noise_reduction },
    { "denoise",        "Denoise",        "Denoise",                  "image-x-generic-symbolic", (const char*[]){ "denoise", "images", NULL },          "denoise",      build_image_denoise       },
    { "sepia",          "Sepia",          "Sepia",                    "image-x-generic-symbolic", (const char*[]){ "images", "sepia", NULL },            "sepia",        build_image_sepia         },
    { "grayscale",      "Grayscale",      "Grayscale",                "image-x-generic-symbolic", (const char*[]){ "grayscale", "images", NULL },        "grayscale",    build_image_grayscale     },
    { "invert",         "Invert",         "Invert",                   "image-x-generic-symbolic", (const char*[]){ "invert", "images", NULL },           "invert",       build_image_invert        },
    { "posterize",      "Posterize",      "Posterize",                "image-x-generic-symbolic", (const char*[]){ "images", "posterize", NULL },        "posterize",    build_image_posterize     },
    { "threshold",      "Threshold",      "Threshold",                "image-x-generic-symbolic", (const char*[]){ "threshold", "images", NULL },        "threshold",    build_image_threshold     },
    { "vignette",       "Vignette",       "Vignette",                 "image-x-generic-symbolic", (const char*[]){ "images", "vignette", NULL },         "vignette",     build_image_vignette      },
    { "film_grain",     "Film Grain",     "Film Grain",               "image-x-generic-symbolic", (const char*[]){ "grain", "images", "film", NULL },    "film-grain",   build_image_film_grain    },
    { "glow",           "Glow",           "Glow",                     "image-x-generic-symbolic", (const char*[]){ "glow", "images", NULL },             "glow",         build_image_glow          },
    { "emboss",         "Emboss",         "Emboss",                   "image-x-generic-symbolic", (const char*[]){ "images", "emboss", NULL },           "emboss",       build_image_emboss        },
    { "edge_detect",    "Edge Detect",    "Edge Detect",              "image-x-generic-symbolic", (const char*[]){ "edge", "detect", "images", NULL },   "edge-detect",  build_image_edge_detect   },
    { "pixelate",       "Pixelate",       "Pixelate",                 "image-x-generic-symbolic", (const char*[]){ "pixelate", "images", NULL },         "pixelate",     build_image_pixelate      },
    { "mosaic",         "Mosaic",         "Mosaic",                   "image-x-generic-symbolic", (const char*[]){ "mosaic", "images", NULL },           "mosaic",       build_image_mosaic        },
    { NULL }
};

static const HelvetiaTool tools_drawing_compositing[] = {
    { "brush_tool",     "Brush Tool",   "Brush Tool",               "image-x-generic-symbolic", (const char*[]){ "images", "tool", "brush", NULL },     "brush-tool",     build_image_brush_tool    },
    { "eraser",         "Eraser",       "Eraser",                   "image-x-generic-symbolic", (const char*[]){ "images", "eraser", NULL },            "eraser",         build_image_eraser        },
    { "fill_bucket",    "Fill / Bucket","Fill / Bucket",            "image-x-generic-symbolic", (const char*[]){ "fill", "images", "bucket", NULL },    "fill-bucket",    build_image_fill_bucket   },
    { "gradient_tool",  "Gradient Tool","Gradient Tool",            "image-x-generic-symbolic", (const char*[]){ "gradient", "tool", "images", NULL },  "gradient-tool",  build_image_gradient_tool },
    { "text_tool",      "Text Tool",    "Text Tool",                "image-x-generic-symbolic", (const char*[]){ "tool", "text", "images", NULL },      "text-tool",      build_image_text_tool     },
    { "shape_tool",     "Shape Tool",   "rectangle, ellipse, line", "image-x-generic-symbolic", (const char*[]){ "images", "tool", "shape", NULL },     "shape-tool",     build_image_shape_tool    },
    { "arrow_tool",     "Arrow Tool",   "Arrow Tool",               "image-x-generic-symbolic", (const char*[]){ "arrow", "tool", "images", NULL },     "arrow-tool",     build_image_arrow_tool    },
    { "eyedropper",     "Eyedropper",   "Eyedropper",               "image-x-generic-symbolic", (const char*[]){ "images", "eyedropper", NULL },        "eyedropper",     build_image_eyedropper    },
    { "clone_stamp",    "Clone Stamp",  "Clone Stamp",              "image-x-generic-symbolic", (const char*[]){ "clone", "images", "stamp", NULL },    "clone-stamp",    build_image_clone_stamp   },
    { "healing_brush",  "Healing Brush","Healing Brush",            "image-x-generic-symbolic", (const char*[]){ "healing", "images", "brush", NULL },  "healing-brush",  build_image_healing_brush },
    { "smudge_tool",    "Smudge Tool",  "Smudge Tool",              "image-x-generic-symbolic", (const char*[]){ "smudge", "tool", "images", NULL },    "smudge-tool",    build_image_smudge_tool   },
    { "dodge_burn",     "Dodge / Burn", "Dodge / Burn",             "image-x-generic-symbolic", (const char*[]){ "dodge", "burn", "images", NULL },     "dodge-burn",     build_image_dodge_burn    },
    { "selection_tools","Selection Tools","rect, ellipse, lasso",   "image-x-generic-symbolic", (const char*[]){ "tools", "selection", "images", NULL },"selection-tools",build_image_selection_tools },
    { "layer_manager",  "Layer Manager","Layer Manager",            "image-x-generic-symbolic", (const char*[]){ "manager", "layer", "images", NULL },  "layer-manager",  build_image_layer_manager },
    { "layer_masks",    "Layer Masks",  "Layer Masks",              "image-x-generic-symbolic", (const char*[]){ "layer", "images", "masks", NULL },    "layer-masks",    build_image_layer_masks   },
    { "blend_modes",    "Blend Modes",  "Blend Modes",              "image-x-generic-symbolic", (const char*[]){ "modes", "images", "blend", NULL },    "blend-modes",    build_image_blend_modes   },
    { "opacity_control","Opacity Control","Opacity Control",        "image-x-generic-symbolic", (const char*[]){ "control", "opacity", "images", NULL },"opacity-control",build_image_opacity_control },
    { NULL }
};

static const HelvetiaTool tools_transform[] = {
    { "scale",          "Scale",          "Scale",          "image-x-generic-symbolic", (const char*[]){ "scale", "images", NULL },         "scale",          build_image_scale          },
    { "skew",           "Skew",           "Skew",           "image-x-generic-symbolic", (const char*[]){ "skew", "images", NULL },          "skew",           build_image_skew           },
    { "distort",        "Distort",        "Distort",        "image-x-generic-symbolic", (const char*[]){ "distort", "images", NULL },       "distort",        build_image_distort        },
    { "warp",           "Warp",           "Warp",           "image-x-generic-symbolic", (const char*[]){ "warp", "images", NULL },          "warp",           build_image_warp           },
    { "free_transform", "Free Transform", "Free Transform", "image-x-generic-symbolic", (const char*[]){ "images", "transform", "free", NULL },"free-transform",build_image_free_transform },
    { NULL }
};

static const HelvetiaTool tools_retouching[] = {
    { "background_removal","Background Removal","Background Removal","image-x-generic-symbolic",(const char*[]){ "background", "removal", "images", NULL },"background-removal",build_image_background_removal },
    { "background_replace","Background Replace","Background Replace","image-x-generic-symbolic",(const char*[]){ "background", "images", "replace", NULL },"background-replace",build_image_background_replace },
    { "portrait_retouch",  "Portrait Retouch",  "Portrait Retouch",  "image-x-generic-symbolic",(const char*[]){ "portrait", "retouch", "images", NULL },  "portrait-retouch",  build_image_portrait_retouch  },
    { "blemish_removal",   "Blemish Removal",   "Blemish Removal",   "image-x-generic-symbolic",(const char*[]){ "blemish", "removal", "images", NULL },    "blemish-removal",   build_image_blemish_removal   },
    { "red_eye_removal",   "Red-eye Removal",   "Red-eye Removal",   "image-x-generic-symbolic",(const char*[]){ "removal", "images", "redeye", NULL },     "red-eye-removal",   build_image_red_eye_removal   },
    { "teeth_whitening",   "Teeth Whitening",   "Teeth Whitening",   "image-x-generic-symbolic",(const char*[]){ "teeth", "whitening", "images", NULL },    "teeth-whitening",   build_image_teeth_whitening   },
    { "skin_smoothing",    "Skin Smoothing",    "Skin Smoothing",    "image-x-generic-symbolic",(const char*[]){ "smoothing", "images", "skin", NULL },     "skin-smoothing",    build_image_skin_smoothing    },
    { NULL }
};

static const HelvetiaTool tools_composition[] = {
    { "collage_maker",   "Collage Maker",   "Collage Maker",   "image-x-generic-symbolic", (const char*[]){ "images", "maker", "collage", NULL },    "collage-maker",    build_image_collage_maker   },
    { "grid_layout",     "Grid Layout",     "Grid Layout",     "image-x-generic-symbolic", (const char*[]){ "layout", "images", "grid", NULL },      "grid-layout",      build_image_grid_layout     },
    { "panorama_stitch", "Panorama Stitch", "Panorama Stitch", "image-x-generic-symbolic", (const char*[]){ "panorama", "images", "stitch", NULL },  "panorama-stitch",  build_image_panorama_stitch },
    { "hdr_merge",       "HDR Merge",       "HDR Merge",       "image-x-generic-symbolic", (const char*[]){ "hdr", "merge", "images", NULL },        "hdr-merge",        build_image_hdr_merge       },
    { "focus_stack",     "Focus Stack",     "Focus Stack",     "image-x-generic-symbolic", (const char*[]){ "images", "focus", "stack", NULL },      "focus-stack",      build_image_focus_stack     },
    { "image_stack",     "Image Stack",     "Image Stack",     "image-x-generic-symbolic", (const char*[]){ "image", "images", "stack", NULL },      "image-stack",      build_image_image_stack     },
    { "watermark",       "Watermark",       "text/image",      "image-x-generic-symbolic", (const char*[]){ "images", "watermark", NULL },           "watermark",        build_image_watermark       },
    { "border_frame",    "Border / Frame",  "Border / Frame",  "image-x-generic-symbolic", (const char*[]){ "frame", "border", "images", NULL },     "border-frame",     build_image_border_frame    },
    { "drop_shadow",     "Drop Shadow",     "Drop Shadow",     "image-x-generic-symbolic", (const char*[]){ "drop", "images", "shadow", NULL },      "drop-shadow",      build_image_drop_shadow     },
    { "reflection",      "Reflection",      "Reflection",      "image-x-generic-symbolic", (const char*[]){ "reflection", "images", NULL },          "reflection",       build_image_reflection      },
    { NULL }
};

static const HelvetiaTool tools_format_conversion[] = {
    { "format_converter",    "Format Converter",   "PNG, JPG, WEBP, AVIF, HEIC, TIFF, BMP, GIF, SVG, ICO","image-x-generic-symbolic",(const char*[]){ "format", "converter", "images", NULL },"format-converter",  build_image_converter         },
    { "batch_convert",       "Batch Convert",      "Batch Convert",     "image-x-generic-symbolic",(const char*[]){ "batch", "convert", "images", NULL },          "batch-convert",     build_image_batch_convert     },
    { "heic_to_jpg",         "HEIC to JPG",        "HEIC to JPG",       "image-x-generic-symbolic",(const char*[]){ "heic", "images", "jpg", NULL },               "heic2jpg",          build_image_heic_to_jpg       },
    { "raw_to_jpg",          "RAW to JPG",         "LibRaw / ImageMagick","image-x-generic-symbolic",(const char*[]){ "images", "raw", "jpg", NULL },             "raw2jpg",           build_image_raw_to_jpg        },
    { "svg_to_png",          "SVG to PNG",         "SVG to PNG",        "image-x-generic-symbolic",(const char*[]){ "png", "svg", "images", NULL },                "svg2png",           build_image_svg_to_png        },
    { "png_to_svg",          "PNG to SVG",         "trace",             "image-x-generic-symbolic",(const char*[]){ "png", "svg", "images", NULL },                "png2svg",           build_image_png_to_svg        },
    { "image_to_base64",     "Image to Base64",    "Image to Base64",   "image-x-generic-symbolic",(const char*[]){ "image", "base64", "images", NULL },           "image2base64",      build_image_to_base64         },
    { "image_compression",   "Image Compression",  "Image Compression", "image-x-generic-symbolic",(const char*[]){ "compression", "image", "images", NULL },      "image-compression", build_image_compression       },
    { "lossless_optimizer",  "Lossless Optimizer", "Lossless Optimizer","image-x-generic-symbolic",(const char*[]){ "lossless", "images", "optimizer", NULL },     "lossless-optimizer",build_image_lossless_optimizer },
    { NULL }
};

static const HelvetiaTool tools_special[] = {
    { "qr_code_reader",       "QR Code Reader",       "QR Code Reader",       "image-x-generic-symbolic",(const char*[]){ "reader", "images", "code", NULL },         "qr-code-reader",       build_image_qr_reader            },
    { "barcode_reader",       "Barcode Reader",       "Barcode Reader",       "image-x-generic-symbolic",(const char*[]){ "barcode", "reader", "images", NULL },       "barcode-reader",       build_image_barcode_reader       },
    { "steganography",        "Steganography",        "hide/extract text",    "image-x-generic-symbolic",(const char*[]){ "images", "steganography", NULL },            "steganography",        build_image_steganography        },
    { "image_to_ascii_art",   "Image to ASCII Art",   "Image to ASCII Art",   "image-x-generic-symbolic",(const char*[]){ "ascii", "art", "image", "images", NULL },   "image2ascii-art",      build_image_to_ascii_art         },
    { "pixel_art_scaler",     "Pixel Art Scaler",     "Pixel Art Scaler",     "image-x-generic-symbolic",(const char*[]){ "scaler", "art", "images", "pixel", NULL },  "pixel-art-scaler",     build_image_pixel_art_scaler     },
    { "sprite_sheet_slicer",  "Sprite Sheet Slicer",  "Sprite Sheet Slicer",  "image-x-generic-symbolic",(const char*[]){ "slicer", "sprite", "images", "sheet", NULL },"sprite-sheet-slicer",  build_image_sprite_sheet_slicer  },
    { "animated_gif_maker",   "Animated GIF Maker",   "Animated GIF Maker",   "image-x-generic-symbolic",(const char*[]){ "gif", "maker", "animated", "images", NULL },"animated-gif-maker",   build_image_animated_gif_maker   },
    { "animated_gif_splitter","Animated GIF Splitter","Animated GIF Splitter","image-x-generic-symbolic",(const char*[]){ "splitter", "gif", "animated", "images", NULL },"animated-gif-splitter",build_image_animated_gif_splitter },
    { "screenshot_tool",      "Screenshot Tool",      "Screenshot Tool",      "image-x-generic-symbolic",(const char*[]){ "screenshot", "tool", "images", NULL },       "screenshot-tool",      build_image_screenshot_tool      },
    { "screen_recorder",      "Screen Recorder",      "GIF/video",            "image-x-generic-symbolic",(const char*[]){ "recorder", "images", "screen", NULL },       "screen-recorder",      build_image_screen_recorder      },
    { NULL }
};

static const HelvetiaSubcategory images_subcategories[] = {
    { "Viewing",               tools_viewing              },
    { "Basic Editing",         tools_basic_editing        },
    { "Adjustments",           tools_adjustments          },
    { "Filters",               tools_filters              },
    { "Drawing & Compositing", tools_drawing_compositing  },
    { "Transform",             tools_transform            },
    { "Retouching",            tools_retouching           },
    { "Composition",           tools_composition          },
    { "Format & Conversion",   tools_format_conversion    },
    { "Special",               tools_special              },
    { NULL, NULL }
};

#include <gegl.h>

static void images_on_activate(void) {
    gegl_init(NULL, NULL);
}

static const HelvetiaModule images_module = {
    .id            = "images",
    .name          = "Images",
    .icon_name     = "image-x-generic-symbolic",
    .description   = "From quick crops to full editing.",
    .subcategories = images_subcategories,
    .create_view   = NULL,
    .on_activate   = images_on_activate,
    .on_deactivate = NULL,
    .on_shutdown   = NULL,
};

const HelvetiaModule *helvetia_images_get_module(void) {
    return &images_module;
}
