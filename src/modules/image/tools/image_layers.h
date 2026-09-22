#pragma once
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "../../../core/tool_registry.h"

G_BEGIN_DECLS

/* ------------------------------------------------------------------ */
/* Blend modes                                                        */
/* ------------------------------------------------------------------ */

typedef enum {
    BLEND_NORMAL = 0,
    BLEND_MULTIPLY,
    BLEND_SCREEN,
    BLEND_OVERLAY,
    BLEND_DARKEN,
    BLEND_LIGHTEN,
    BLEND_DIFFERENCE,
    BLEND_ADD,
    BLEND_SOFT_LIGHT,
    BLEND_HARD_LIGHT,
    BLEND_COUNT
} BlendMode;

const char *blend_mode_name(BlendMode m);

/* ------------------------------------------------------------------ */
/* Selection mask                                                     */
/* ------------------------------------------------------------------ */

typedef struct {
    int      width;
    int      height;
    guchar  *mask;      /* 0 = excluded, 255 = included */
    gboolean active;    /* if FALSE, all pixels are selected */
    char     shape[16]; /* "none" / "rect" / "ellipse" / "lasso" / "wand" */
} SelectionMask;

SelectionMask *selection_new(int w, int h);
void           selection_free(SelectionMask *s);
void           selection_reset(SelectionMask *s);   /* full select */
void           selection_none(SelectionMask *s);    /* select nothing */
gboolean       selection_is_selected(SelectionMask *s, int x, int y);
guchar         selection_weight(SelectionMask *s, int x, int y);

/* Shape builders */
void selection_fill_rect(SelectionMask *s, int x0, int y0, int x1, int y1);
void selection_fill_ellipse(SelectionMask *s, int x0, int y0, int x1, int y1);
void selection_fill_polygon(SelectionMask *s, const int *xs, const int *ys, int n);
void selection_flood_from(SelectionMask *s, GdkPixbuf *pb,
                           int x, int y, int tolerance);

/* ------------------------------------------------------------------ */
/* Layer                                                              */
/* ------------------------------------------------------------------ */

typedef struct {
    char      *name;
    GdkPixbuf *pixbuf;      /* RGBA */
    guchar    *mask;        /* optional, w*h, NULL = no mask */
    gboolean   visible;
    double     opacity;     /* 0..1 */
    BlendMode  blend;
} Layer;

Layer *layer_new(const char *name, GdkPixbuf *pixbuf);
Layer *layer_new_empty(int w, int h, const char *name);
void   layer_free(Layer *l);
GdkPixbuf *layer_ensure_rgba(Layer *l);

/* ------------------------------------------------------------------ */
/* LayerStack                                                         */
/* ------------------------------------------------------------------ */

typedef struct {
    GPtrArray *layers;      /* Layer* */
    int        active;      /* index of currently selected layer */
    int        width;
    int        height;
} LayerStack;

LayerStack *layer_stack_new(int w, int h);
void        layer_stack_free(LayerStack *ls);

void        layer_stack_add(LayerStack *ls, Layer *l);       /* on top */
void        layer_stack_insert(LayerStack *ls, Layer *l, int idx);
void        layer_stack_remove(LayerStack *ls, int idx);
void        layer_stack_move(LayerStack *ls, int from, int to);
Layer      *layer_stack_get(LayerStack *ls, int idx);
Layer      *layer_stack_active(LayerStack *ls);
void        layer_stack_set_active(LayerStack *ls, int idx);

/* Composite the entire stack bottom-to-top into a new pixbuf */
GdkPixbuf  *layer_stack_composite(LayerStack *ls);

/* ------------------------------------------------------------------ */
/* Blend math (exposed for tools)                                     */
/* ------------------------------------------------------------------ */

guchar blend_channel(BlendMode mode, guchar base, guchar top);

/* ------------------------------------------------------------------ */
/* Editor factories                                                   */
/* ------------------------------------------------------------------ */

GtkWidget *image_selection_tools_create(void);
GtkWidget *image_layer_manager_create(void);
GtkWidget *image_layer_masks_create(void);
GtkWidget *image_blend_modes_create(void);

extern const HelvetiaToolCommand image_selection_tools_commands[];
extern const HelvetiaToolCommand image_layer_manager_commands[];
extern const HelvetiaToolCommand image_layer_masks_commands[];
extern const HelvetiaToolCommand image_blend_modes_commands[];

void image_selection_tools_on_close(GtkWidget *view);
void image_layer_manager_on_close(GtkWidget *view);
void image_layer_masks_on_close(GtkWidget *view);
void image_blend_modes_on_close(GtkWidget *view);

G_END_DECLS
