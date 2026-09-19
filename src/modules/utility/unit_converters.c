#include "utility_module.h"
#include <gtk/gtk.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *name;
    double to_base; /* multiplier to convert to Joules */
} UnitDef;

/* Comprehensive list of energy units */
static const UnitDef energy_units[] = {
    { "Joule", 1.0 },
    { "Kilojoule", 1e3 },
    { "Megajoule", 1e6 },
    { "Gigajoule", 1e9 },
    { "Terajoule", 1e12 },
    { "Millijoule", 1e-3 },
    { "Microjoule", 1e-6 },
    { "Nanojoule", 1e-9 },
    { "Picojoule", 1e-12 },
    { "Femtojoule", 1e-15 },
    { "Attojoule", 1e-18 },
    { "Electronvolt", 1.602176634e-19 },
    { "Attoelectron volt", 1.602176634e-37 },
    { "Femtoelectron volt", 1.602176634e-34 },
    { "Picoelectron volt", 1.602176634e-31 },
    { "Nanoelectron volt", 1.602176634e-28 },
    { "Microelectron volt", 1.602176634e-25 },
    { "Millielectron volt", 1.602176634e-22 },
    { "Centielectron volt", 1.602176634e-21 },
    { "Decielectron volt", 1.602176634e-20 },
    { "Decaelectron volt", 1.602176634e-18 },
    { "Hectoelectron volt", 1.602176634e-17 },
    { "Kiloelectron volt", 1.602176634e-16 },
    { "Megaelectron volt", 1.602176634e-13 },
    { "Gigaelectron volt", 1.602176634e-10 },
    { "Teraelectron volt", 1.602176634e-7 },
    { "Petaelectron volt", 1.602176634e-4 },
    { "Exaelectron volt", 0.1602176634 },
    { "Calorie (th)", 4.184 },
    { "Kilocalorie (th)", 4184.0 },
    { "Calorie (nutritional)", 4184.0 },
    { "BTU", 1055.05585262 },
    { "Erg", 1e-7 },
    { "Watt Hour", 3600.0 },
    { "Kilowatt Hour", 3600000.0 },
    { "Megawatt Hour", 3.6e9 },
    { "Gigawatt Hour", 3.6e12 }
};

static const size_t num_energy_units = sizeof(energy_units) / sizeof(energy_units[0]);

/* Sorting function for unit names */
static int compare_units(const void *a, const void *b) {
    const UnitDef *ua = (const UnitDef *)a;
    const UnitDef *ub = (const UnitDef *)b;
    return g_utf8_collate(ua->name, ub->name);
}

typedef struct {
    GtkWidget *e1, *e2;
    GtkWidget *dd1, *dd2;
    UnitDef *units;
    size_t num_units;
    gboolean updating; /* prevent recursive updates */
    int last_edited; /* 1 or 2 */
} ConverterCtx;

static void update_conversion(ConverterCtx *ctx) {
    if (ctx->updating) return;
    ctx->updating = TRUE;

    guint idx1 = gtk_drop_down_get_selected(GTK_DROP_DOWN(ctx->dd1));
    guint idx2 = gtk_drop_down_get_selected(GTK_DROP_DOWN(ctx->dd2));
    
    if (idx1 >= ctx->num_units || idx2 >= ctx->num_units) {
        ctx->updating = FALSE;
        return;
    }

    double to_base_1 = ctx->units[idx1].to_base;
    double to_base_2 = ctx->units[idx2].to_base;

    if (ctx->last_edited == 1) {
        const char *txt = gtk_editable_get_text(GTK_EDITABLE(ctx->e1));
        if (txt && *txt) {
            double val1 = g_ascii_strtod(txt, NULL);
            double val2 = (val1 * to_base_1) / to_base_2;
            char buf[64];
            snprintf(buf, sizeof(buf), "%.10g", val2);
            gtk_editable_set_text(GTK_EDITABLE(ctx->e2), buf);
        } else {
            gtk_editable_set_text(GTK_EDITABLE(ctx->e2), "");
        }
    } else {
        const char *txt = gtk_editable_get_text(GTK_EDITABLE(ctx->e2));
        if (txt && *txt) {
            double val2 = g_ascii_strtod(txt, NULL);
            double val1 = (val2 * to_base_2) / to_base_1;
            char buf[64];
            snprintf(buf, sizeof(buf), "%.10g", val1);
            gtk_editable_set_text(GTK_EDITABLE(ctx->e1), buf);
        } else {
            gtk_editable_set_text(GTK_EDITABLE(ctx->e1), "");
        }
    }

    ctx->updating = FALSE;
}

static void on_e1_changed(GtkEditable *e, gpointer ud) {
    (void)e;
    ConverterCtx *ctx = ud;
    if (ctx->updating) return;
    ctx->last_edited = 1;
    update_conversion(ctx);
}

static void on_e2_changed(GtkEditable *e, gpointer ud) {
    (void)e;
    ConverterCtx *ctx = ud;
    if (ctx->updating) return;
    ctx->last_edited = 2;
    update_conversion(ctx);
}

static void on_unit_changed(GtkDropDown *dd, GParamSpec *ps, gpointer ud) {
    (void)dd; (void)ps;
    ConverterCtx *ctx = ud;
    update_conversion(ctx);
}

static void on_swap_clicked(GtkButton *btn, gpointer ud) {
    (void)btn;
    ConverterCtx *ctx = ud;
    
    ctx->updating = TRUE;
    guint idx1 = gtk_drop_down_get_selected(GTK_DROP_DOWN(ctx->dd1));
    guint idx2 = gtk_drop_down_get_selected(GTK_DROP_DOWN(ctx->dd2));
    gtk_drop_down_set_selected(GTK_DROP_DOWN(ctx->dd1), idx2);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(ctx->dd2), idx1);
    ctx->updating = FALSE;
    
    update_conversion(ctx);
}

static void ctx_free(gpointer ud) {
    ConverterCtx *ctx = ud;
    g_free(ctx->units);
    g_free(ctx);
}

static GtkWidget *build_generic_layout(const UnitDef *base_units, size_t num) {
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    /* Center the grid horizontally */
    gtk_widget_set_halign(grid, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(grid, 20);

    ConverterCtx *ctx = g_new0(ConverterCtx, 1);
    ctx->num_units = num;
    ctx->units = g_memdup2(base_units, num * sizeof(UnitDef));
    ctx->last_edited = 1;
    
    /* Sort units alphabetically */
    qsort(ctx->units, num, sizeof(UnitDef), compare_units);

    const char **names = g_new0(const char*, num + 1);
    for (size_t i = 0; i < num; i++) {
        names[i] = ctx->units[i].name;
    }
    names[num] = NULL;

    GtkStringList *model1 = gtk_string_list_new(names);
    GtkStringList *model2 = gtk_string_list_new(names);
    g_free(names);

    ctx->e1 = gtk_entry_new();
    ctx->e2 = gtk_entry_new();
    gtk_widget_set_hexpand(ctx->e1, TRUE);
    gtk_widget_set_hexpand(ctx->e2, TRUE);
    /* Make entries decently wide */
    gtk_widget_set_size_request(ctx->e1, 200, -1);
    gtk_widget_set_size_request(ctx->e2, 200, -1);
    
    /* Set alignment for entries (like the screenshot, text looks left-aligned) */
    gtk_editable_set_alignment(GTK_EDITABLE(ctx->e1), 0.0);
    gtk_editable_set_alignment(GTK_EDITABLE(ctx->e2), 0.0);

    ctx->dd1 = gtk_drop_down_new(G_LIST_MODEL(model1), gtk_property_expression_new(GTK_TYPE_STRING_OBJECT, NULL, "string"));
    ctx->dd2 = gtk_drop_down_new(G_LIST_MODEL(model2), gtk_property_expression_new(GTK_TYPE_STRING_OBJECT, NULL, "string"));
    gtk_drop_down_set_enable_search(GTK_DROP_DOWN(ctx->dd1), TRUE);
    gtk_drop_down_set_enable_search(GTK_DROP_DOWN(ctx->dd2), TRUE);
    gtk_widget_set_hexpand(ctx->dd1, TRUE);
    gtk_widget_set_hexpand(ctx->dd2, TRUE);

    GtkWidget *swap_btn = gtk_button_new_from_icon_name("object-flip-horizontal-symbolic");
    gtk_widget_set_valign(swap_btn, GTK_ALIGN_CENTER);
    /* Remove background from swap button to make it look like just an icon if desired,
       but standard button is fine. Let's make it flat. */
    gtk_widget_add_css_class(swap_btn, "flat");

    /* Layout */
    gtk_grid_attach(GTK_GRID(grid), ctx->e1, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), ctx->e2, 2, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), ctx->dd1, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), swap_btn, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), ctx->dd2, 2, 1, 1, 1);

    /* Connections */
    g_signal_connect(ctx->e1, "changed", G_CALLBACK(on_e1_changed), ctx);
    g_signal_connect(ctx->e2, "changed", G_CALLBACK(on_e2_changed), ctx);
    g_signal_connect(ctx->dd1, "notify::selected", G_CALLBACK(on_unit_changed), ctx);
    g_signal_connect(ctx->dd2, "notify::selected", G_CALLBACK(on_unit_changed), ctx);
    g_signal_connect(swap_btn, "clicked", G_CALLBACK(on_swap_clicked), ctx);
    g_signal_connect_swapped(grid, "destroy", G_CALLBACK(ctx_free), ctx);
    
    /* Default selections */
    guint default_idx1 = 0;
    guint default_idx2 = 1;
    /* Find Joule and Watt Hour as defaults for energy */
    for (size_t i = 0; i < num; i++) {
        if (strcmp(ctx->units[i].name, "Joule") == 0) default_idx1 = i;
        if (strcmp(ctx->units[i].name, "Watt Hour") == 0) default_idx2 = i;
    }
    gtk_drop_down_set_selected(GTK_DROP_DOWN(ctx->dd1), default_idx1);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(ctx->dd2), default_idx2);
    
    /* Set initial value */
    gtk_editable_set_text(GTK_EDITABLE(ctx->e1), "1");

    return grid;
}

GtkWidget *build_energy_converter(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *grid = build_generic_layout(energy_units, num_energy_units);
    gtk_box_append(GTK_BOX(box), grid);
    return box;
}
