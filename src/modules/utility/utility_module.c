/* ================================================================
 * Helvetia — Utility Module
 *
 * Tools included:
 *   1. Base Converter      (Bin / Oct / Dec / Hex)
 *   2. Hash Generator      (MD5, SHA-1, SHA-256, SHA-512 via GLib)
 *   3. Text Case Converter (UPPER / lower / Title / sWAP)
 *   4. UUID Generator      (random v4 via GLib)
 *   5. Unit Converter      (length / weight / temperature)
 *   6. Color Tool          (HEX <-> RGB)
 * ================================================================ */
#include "utility_module.h"
#include "../../ui/window.h"
#include "color_studio.h"
#include "qr_studio.h"
#include "timezone_studio.h"

#include <gtk/gtk.h>
#include <glib.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

/* Closure-compatible free wrapper for GClosureNotify */
static void closure_free(gpointer data, GClosure *closure) {
    (void)closure;
    g_free(data);
}

/* ----------------------------------------------------------------
 * Helper — Make a mono-styled result label
 * ---------------------------------------------------------------- */
static GtkWidget *make_result_label(void) {
    GtkWidget *lbl = gtk_label_new("—");
    gtk_widget_add_css_class(lbl, "helvetia-result");
    gtk_label_set_selectable(GTK_LABEL(lbl), TRUE);
    gtk_label_set_wrap(GTK_LABEL(lbl), TRUE);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
    return lbl;
}

/* ================================================================
 * Tool 1 — Base Converter
 * ================================================================ */
typedef struct { GtkWidget *entry, *from_dd, *to_dd, *result; } BaseCtx;

static void on_base_convert(GtkButton *btn, gpointer user_data) {
    (void)btn;
    BaseCtx *ctx = user_data;
    const char *text = gtk_editable_get_text(GTK_EDITABLE(ctx->entry));

    static const int bases[] = { 2, 8, 10, 16 };
    guint from_idx = gtk_drop_down_get_selected(GTK_DROP_DOWN(ctx->from_dd));
    guint to_idx   = gtk_drop_down_get_selected(GTK_DROP_DOWN(ctx->to_dd));

    char *end = NULL;
    long long value = strtoll(text, &end, bases[from_idx]);
    if (!end || end == text) {
        gtk_label_set_text(GTK_LABEL(ctx->result), "⚠ Invalid input");
        return;
    }

    char buf[128];
    switch (to_idx) {
        case 0: { /* binary */
            if (value == 0) { strcpy(buf, "0"); break; }
            char tmp[66] = {0};
            int  pos = 64;
            long long v = value;
            while (v > 0) { tmp[pos--] = '0' + (v & 1); v >>= 1; }
            strcpy(buf, tmp + pos + 1);
            break;
        }
        case 1: snprintf(buf, sizeof buf, "%llo",  value); break;
        case 2: snprintf(buf, sizeof buf, "%lld",  value); break;
        default:snprintf(buf, sizeof buf, "0x%llX",value); break;
    }
    gtk_label_set_text(GTK_LABEL(ctx->result), buf);
}

static GtkWidget *build_base_converter(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Enter a number…");

    const char *items[] = { "Binary (2)", "Octal (8)", "Decimal (10)", "Hex (16)", NULL };
    GtkWidget *from = gtk_drop_down_new_from_strings(items);
    GtkWidget *to   = gtk_drop_down_new_from_strings(items);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(from), 2); /* default: decimal */
    gtk_drop_down_set_selected(GTK_DROP_DOWN(to),   3); /* to: hex          */

    GtkWidget *from_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(from_box), gtk_label_new("From:"));
    gtk_box_append(GTK_BOX(from_box), from);

    GtkWidget *to_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(to_box), gtk_label_new("To:  "));
    gtk_box_append(GTK_BOX(to_box), to);

    GtkWidget *btn    = gtk_button_new_with_label("Convert");
    gtk_widget_add_css_class(btn, "suggested-action");
    GtkWidget *result = make_result_label();

    BaseCtx *ctx  = g_new0(BaseCtx, 1);
    ctx->entry    = entry;
    ctx->from_dd  = from;
    ctx->to_dd    = to;
    ctx->result   = result;
    g_signal_connect_data(btn, "clicked", G_CALLBACK(on_base_convert),
                          ctx, closure_free, 0);
    /* Also convert on Enter */
    g_signal_connect_data(entry, "activate", G_CALLBACK(on_base_convert),
                          ctx, NULL, 0);

    gtk_box_append(GTK_BOX(box), entry);
    gtk_box_append(GTK_BOX(box), from_box);
    gtk_box_append(GTK_BOX(box), to_box);
    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), result);

    return box;
}

/* ================================================================
 * Tool 2 — Hash Generator
 * ================================================================ */
typedef struct { GtkWidget *entry, *result; GChecksumType type; } HashCtx;

static void on_hash_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    HashCtx *ctx  = user_data;
    const char *t = gtk_editable_get_text(GTK_EDITABLE(ctx->entry));
    GChecksum *cs = g_checksum_new(ctx->type);
    g_checksum_update(cs, (const guchar *)t, (gssize)strlen(t));
    gtk_label_set_text(GTK_LABEL(ctx->result), g_checksum_get_string(cs));
    g_checksum_free(cs);
}

static GtkWidget *build_hash_tool(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Text to hash…");

    /* Button row */
    GtkWidget *btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *result  = make_result_label();

    static const struct { const char *label; GChecksumType type; } algs[] = {
        { "MD5",    G_CHECKSUM_MD5    },
        { "SHA-1",  G_CHECKSUM_SHA1   },
        { "SHA-256",G_CHECKSUM_SHA256 },
        { "SHA-512",G_CHECKSUM_SHA512 },
    };

    for (gsize i = 0; i < G_N_ELEMENTS(algs); i++) {
        GtkWidget *b = gtk_button_new_with_label(algs[i].label);
        HashCtx *ctx = g_new0(HashCtx, 1);
        ctx->entry   = entry;
        ctx->result  = result;
        ctx->type    = algs[i].type;
        g_signal_connect_data(b, "clicked", G_CALLBACK(on_hash_clicked),
                              ctx, closure_free, 0);
        gtk_box_append(GTK_BOX(btn_row), b);
    }

    gtk_box_append(GTK_BOX(box), entry);
    gtk_box_append(GTK_BOX(box), btn_row);
    gtk_box_append(GTK_BOX(box), result);

    return box;
}

/* ================================================================
 * Tool 3 — Text Case Converter
 * ================================================================ */
typedef struct { GtkWidget *entry, *result; int mode; } CaseCtx;

static void on_case_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    CaseCtx *ctx  = user_data;
    const char *t = gtk_editable_get_text(GTK_EDITABLE(ctx->entry));
    char *out     = g_strdup(t);
    size_t len    = strlen(out);

    switch (ctx->mode) {
        case 0: /* UPPER */
            for (size_t i = 0; i < len; i++) out[i] = (char)toupper((unsigned char)out[i]);
            break;
        case 1: /* lower */
            for (size_t i = 0; i < len; i++) out[i] = (char)tolower((unsigned char)out[i]);
            break;
        case 2: { /* Title Case */
            gboolean start = TRUE;
            for (size_t i = 0; i < len; i++) {
                if (out[i] == ' ') { start = TRUE; }
                else if (start)    { out[i] = (char)toupper((unsigned char)out[i]); start = FALSE; }
                else               { out[i] = (char)tolower((unsigned char)out[i]); }
            }
            break;
        }
        case 3: /* sWAP */
            for (size_t i = 0; i < len; i++) {
                if (isupper((unsigned char)out[i]))      out[i] = (char)tolower((unsigned char)out[i]);
                else if (islower((unsigned char)out[i])) out[i] = (char)toupper((unsigned char)out[i]);
            }
            break;
    }

    gtk_label_set_text(GTK_LABEL(ctx->result), out);
    g_free(out);
}

static GtkWidget *build_case_tool(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Type or paste text…");

    GtkWidget *result  = make_result_label();
    GtkWidget *btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);

    static const struct { const char *lbl; int mode; } modes[] = {
        { "UPPER", 0 }, { "lower", 1 }, { "Title", 2 }, { "sWAP", 3 },
    };

    for (gsize i = 0; i < G_N_ELEMENTS(modes); i++) {
        GtkWidget *b = gtk_button_new_with_label(modes[i].lbl);
        CaseCtx *ctx = g_new0(CaseCtx, 1);
        ctx->entry   = entry;
        ctx->result  = result;
        ctx->mode    = modes[i].mode;
        g_signal_connect_data(b, "clicked", G_CALLBACK(on_case_clicked),
                              ctx, closure_free, 0);
        gtk_box_append(GTK_BOX(btn_row), b);
    }

    gtk_box_append(GTK_BOX(box), entry);
    gtk_box_append(GTK_BOX(box), btn_row);
    gtk_box_append(GTK_BOX(box), result);

    return box;
}

/* ================================================================
 * Tool 4 — UUID v4 Generator
 * ================================================================ */
static void on_uuid_generate(GtkButton *btn, gpointer user_data) {
    (void)btn;
    GtkWidget *lbl = user_data;
    /* GLib 2.52+ has g_uuid_string_random() */
    gchar *uuid = g_uuid_string_random();
    gtk_label_set_text(GTK_LABEL(lbl), uuid);
    g_free(uuid);
}

static void on_uuid_copy(GtkButton *btn, gpointer user_data) {
    (void)btn;
    GtkWidget *lbl = user_data;
    GdkClipboard *clip = gdk_display_get_clipboard(gdk_display_get_default());
    gdk_clipboard_set_text(clip, gtk_label_get_text(GTK_LABEL(lbl)));
}

static GtkWidget *build_uuid_tool(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    GtkWidget *result = make_result_label();
    gtk_label_set_text(GTK_LABEL(result), "Click Generate…");

    GtkWidget *btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *gen  = gtk_button_new_with_label("⟳  Generate");
    GtkWidget *copy = gtk_button_new_with_label("⎘  Copy");
    gtk_widget_add_css_class(gen, "suggested-action");

    g_signal_connect(gen,  "clicked", G_CALLBACK(on_uuid_generate), result);
    g_signal_connect(copy, "clicked", G_CALLBACK(on_uuid_copy),     result);

    gtk_box_append(GTK_BOX(btn_row), gen);
    gtk_box_append(GTK_BOX(btn_row), copy);

    gtk_box_append(GTK_BOX(box), btn_row);
    gtk_box_append(GTK_BOX(box), result);

    return box;
}

/* ================================================================
 * Tool 5 — Unit Converter (length / weight / temperature)
 * ================================================================ */
typedef struct {
    GtkWidget *value_entry, *from_dd, *to_dd, *cat_dd, *result;
    GtkStringList *from_model, *to_model;
} UnitCtx;

/* Category: 0=Length, 1=Weight, 2=Temperature */
static const char *length_units[] = {
    "Millimetre","Centimetre","Metre","Kilometre","Inch","Foot","Yard","Mile",NULL
};
static const double length_to_m[] = {
    0.001, 0.01, 1.0, 1000.0, 0.0254, 0.3048, 0.9144, 1609.344
};

static const char *weight_units[] = {
    "Milligram","Gram","Kilogram","Tonne","Ounce","Pound","Stone",NULL
};
static const double weight_to_kg[] = {
    0.000001, 0.001, 1.0, 1000.0, 0.0283495, 0.453592, 6.35029
};

static const char *temp_units[] = { "Celsius","Fahrenheit","Kelvin",NULL };

static void on_unit_convert(GtkButton *btn, gpointer user_data) {
    (void)btn;
    UnitCtx *ctx  = user_data;
    const char *vs = gtk_editable_get_text(GTK_EDITABLE(ctx->value_entry));
    double val    = strtod(vs, NULL);
    guint cat     = gtk_drop_down_get_selected(GTK_DROP_DOWN(ctx->cat_dd));
    guint from    = gtk_drop_down_get_selected(GTK_DROP_DOWN(ctx->from_dd));
    guint to      = gtk_drop_down_get_selected(GTK_DROP_DOWN(ctx->to_dd));

    double result = 0.0;
    char buf[128];

    if (cat == 0) { /* Length */
        result = val * length_to_m[from] / length_to_m[to];
        snprintf(buf, sizeof buf, "%.6g", result);
    } else if (cat == 1) { /* Weight */
        result = val * weight_to_kg[from] / weight_to_kg[to];
        snprintf(buf, sizeof buf, "%.6g", result);
    } else { /* Temperature */
        double celsius;
        switch (from) {
            case 0: celsius = val;               break;
            case 1: celsius = (val - 32.0)/1.8;  break;
            default:celsius = val - 273.15;       break;
        }
        switch (to) {
            case 0: result = celsius;                    break;
            case 1: result = celsius * 1.8 + 32.0;      break;
            default:result = celsius + 273.15;           break;
        }
        snprintf(buf, sizeof buf, "%.4g", result);
    }

    gtk_label_set_text(GTK_LABEL(ctx->result), buf);
}

static void on_unit_category_changed(GtkDropDown *dd, GParamSpec *ps,
                                     gpointer user_data)
{
    (void)ps;
    UnitCtx *ctx = user_data;
    guint cat    = gtk_drop_down_get_selected(dd);

    const char **list = NULL;
    if      (cat == 0) list = length_units;
    else if (cat == 1) list = weight_units;
    else               list = temp_units;

    guint n_items = g_list_model_get_n_items(G_LIST_MODEL(ctx->from_model));
    gtk_string_list_splice(ctx->from_model, 0, n_items, list);
    
    n_items = g_list_model_get_n_items(G_LIST_MODEL(ctx->to_model));
    gtk_string_list_splice(ctx->to_model, 0, n_items, list);
}

static GtkWidget *build_unit_converter(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    const char *categories[] = { "Length", "Weight", "Temperature", NULL };
    GtkWidget *cat_dd = gtk_drop_down_new_from_strings(categories);

    /* Each drop-down must own a separate model instance */
    GtkStringList *from_model = gtk_string_list_new(length_units);
    GtkStringList *to_model   = gtk_string_list_new(length_units);
    GtkWidget *from_dd = gtk_drop_down_new(G_LIST_MODEL(from_model), NULL);
    GtkWidget *to_dd   = gtk_drop_down_new(G_LIST_MODEL(to_model),   NULL);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(to_dd), 4); /* default to: inch */

    GtkWidget *value_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(value_entry), "Value…");

    GtkWidget *result = make_result_label();
    GtkWidget *btn    = gtk_button_new_with_label("Convert");
    gtk_widget_add_css_class(btn, "suggested-action");

    UnitCtx *ctx         = g_new0(UnitCtx, 1);
    ctx->value_entry     = value_entry;
    ctx->from_dd         = from_dd;
    ctx->to_dd           = to_dd;
    ctx->cat_dd          = cat_dd;
    ctx->result          = result;
    ctx->from_model      = from_model;
    ctx->to_model        = to_model;
    /* Clean up the models when ctx is freed */

    g_signal_connect(cat_dd, "notify::selected",
                     G_CALLBACK(on_unit_category_changed), ctx);
    g_signal_connect(btn, "clicked", G_CALLBACK(on_unit_convert), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_object_unref), from_model);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_object_unref), to_model);
    g_signal_connect_data(value_entry, "activate", G_CALLBACK(on_unit_convert),
                          ctx, NULL, 0);

    GtkWidget *cat_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(cat_row), gtk_label_new("Category:"));
    gtk_box_append(GTK_BOX(cat_row), cat_dd);

    GtkWidget *from_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(from_row), gtk_label_new("From:"));
    gtk_box_append(GTK_BOX(from_row), from_dd);

    GtkWidget *to_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(to_row), gtk_label_new("To:  "));
    gtk_box_append(GTK_BOX(to_row), to_dd);

    gtk_box_append(GTK_BOX(box), cat_row);
    gtk_box_append(GTK_BOX(box), value_entry);
    gtk_box_append(GTK_BOX(box), from_row);
    gtk_box_append(GTK_BOX(box), to_row);
    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), result);

    return box;
}

/* ================================================================
 * Module view — 2-column responsive grid of cards
 * ================================================================ */
/* ================================================================
 * Module Tools & Subcategories
 * ================================================================ */
static const char *kw_base[] = {"base", "binary", "octal", "decimal", "hex", "converter", NULL};
static const char *kw_hash[] = {"hash", "md5", "sha1", "sha256", "sha512", "generator", NULL};
static const char *kw_case[] = {"case", "text", "upper", "lower", "title", "swap", NULL};
static const char *kw_uuid[] = {"uuid", "guid", "generator", "random", NULL};
static const char *kw_unit[] = {"unit", "length", "weight", "temperature", "converter", NULL};
static const char *kw_color[] = {"color", "hex", "rgb", "picker", "converter", NULL};

static const HelvetiaTool tools_math[] = {
    { "calc_sci", "Scientific Calculator", "Advanced math operations", "accessories-calculator-symbolic", NULL, "calc", build_calc_sci },
    { "calc_frac", "Fraction Calculator", "Fractions operations", "accessories-calculator-symbolic", NULL, "frac", build_frac_calc },
    { "calc_pct", "Percentage Calculator", "Quick percentage math", "accessories-calculator-symbolic", NULL, "pct", build_pct_calc },
    { "rand_num", "Random Number Generator", "Generate random numbers", "media-playlist-shuffle-symbolic", NULL, "rand", build_generic_random_tool },
    { "calc_pct_err", "Percent Error Calculator", "Calculate percent error", "accessories-calculator-symbolic", NULL, "pcterr", build_coming_soon },
    { "calc_exp", "Exponent Calculator", "Calculate exponents", "accessories-calculator-symbolic", NULL, "exp", build_coming_soon },
    { "calc_bin", "Binary Calculator", "Binary operations", "format-text-numeric-symbolic", NULL, "bin", build_coming_soon },
    { "calc_hex", "Hex Calculator", "Hex operations", "format-text-numeric-symbolic", NULL, "hex", build_coming_soon },
    { "calc_halflife", "Half-Life Calculator", "Calculate half-life", "accessories-calculator-symbolic", NULL, "halflife", build_coming_soon },
    { "calc_quad", "Quadratic Formula Calculator", "Solve quadratic equations", "accessories-calculator-symbolic", NULL, "quad", build_coming_soon },
    { "calc_log", "Log Calculator", "Logarithms", "accessories-calculator-symbolic", NULL, "log", build_coming_soon },
    { "calc_ratio", "Ratio Calculator", "Calculate ratios", "accessories-calculator-symbolic", NULL, "ratio", build_coming_soon },
    { "calc_root", "Root Calculator", "Calculate roots", "accessories-calculator-symbolic", NULL, "root", build_coming_soon },
    { "calc_lcm", "Least Common Multiple", "Calculate LCM", "accessories-calculator-symbolic", NULL, "lcm", build_coming_soon },
    { "calc_gcf", "Greatest Common Factor", "Calculate GCF", "accessories-calculator-symbolic", NULL, "gcf", build_coming_soon },
    { "calc_factor", "Factor Calculator", "Find factors", "accessories-calculator-symbolic", NULL, "factor", build_coming_soon },
    { "calc_round", "Rounding Calculator", "Round numbers", "accessories-calculator-symbolic", NULL, "round", build_coming_soon },
    { "calc_matrix", "Matrix Calculator", "Matrix operations", "accessories-calculator-symbolic", NULL, "matrix", build_coming_soon },
    { "calc_scinote", "Scientific Notation Calculator", "Scientific notation", "accessories-calculator-symbolic", NULL, "scinote", build_coming_soon },
    { "calc_bignum", "Big Number Calculator", "Big number math", "accessories-calculator-symbolic", NULL, "bignum", build_coming_soon },
    { "base_convert", "Number Base Converter", "Bin · Oct · Dec · Hex", "format-text-numeric-symbolic", kw_base, "base", build_base_converter },
    { "roman_num", "Roman Numeral", "Convert to/from Roman numerals", "format-text-numeric-symbolic", NULL, "roman", build_generic_math_tool },
    { .id = NULL }
};

static const HelvetiaTool tools_statistics[] = {
    { "calc_stddev", "Standard Deviation Calculator", "Calculate std deviation", "accessories-calculator-symbolic", NULL, "stddev", build_stddev_calc },
    { "calc_seq", "Number Sequence Calculator", "Calculate sequences", "accessories-calculator-symbolic", NULL, "seq", build_coming_soon },
    { "calc_samplesize", "Sample Size Calculator", "Calculate sample size", "accessories-calculator-symbolic", NULL, "samplesize", build_coming_soon },
    { "calc_prob", "Probability Calculator", "Calculate probability", "accessories-calculator-symbolic", NULL, "prob", build_coming_soon },
    { "calc_stats", "Statistics Calculator", "Calculate stats", "accessories-calculator-symbolic", NULL, "stats", build_coming_soon },
    { "calc_mean", "Mean, Median, Mode, Range", "Calculate averages", "accessories-calculator-symbolic", NULL, "mean", build_coming_soon },
    { "calc_perm", "Permutation and Combination", "Calculate permutations", "accessories-calculator-symbolic", NULL, "perm", build_coming_soon },
    { "calc_zscore", "Z-score Calculator", "Calculate Z-score", "accessories-calculator-symbolic", NULL, "zscore", build_coming_soon },
    { "calc_conf", "Confidence Interval Calculator", "Calculate confidence interval", "accessories-calculator-symbolic", NULL, "conf", build_coming_soon },
    { .id = NULL }
};

static const HelvetiaTool tools_geometry[] = {
    { "calc_triangle", "Triangle Calculator", "Calculate triangle properties", "accessories-calculator-symbolic", NULL, "triangle", build_tri_calc },
    { "calc_volume", "Volume Calculator", "Calculate volume", "accessories-calculator-symbolic", NULL, "volume", build_vol_calc },
    { "calc_slope", "Slope Calculator", "Calculate slope", "accessories-calculator-symbolic", NULL, "slope", build_coming_soon },
    { "calc_area", "Area Calculator", "Calculate area", "accessories-calculator-symbolic", NULL, "area", build_coming_soon },
    { "calc_dist", "Distance Calculator", "Calculate distance", "accessories-calculator-symbolic", NULL, "dist", build_coming_soon },
    { "calc_circle", "Circle Calculator", "Calculate circle properties", "accessories-calculator-symbolic", NULL, "circle", build_coming_soon },
    { "calc_surf", "Surface Area Calculator", "Calculate surface area", "accessories-calculator-symbolic", NULL, "surf", build_coming_soon },
    { "calc_pythag", "Pythagorean Theorem", "Calculate hypotenuse", "accessories-calculator-symbolic", NULL, "pythag", build_coming_soon },
    { "calc_righttri", "Right Triangle Calculator", "Calculate right triangles", "accessories-calculator-symbolic", NULL, "righttri", build_coming_soon },
    { .id = NULL }
};

static const HelvetiaTool tools_units[] = {
    { "unit_convert", "Unit Converter", "Length · Weight · Temperature", "view-sort-ascending-symbolic", kw_unit, "unit", build_unit_converter },
    { "temp_convert", "Temperature Converter", "C · F · K", "view-sort-ascending-symbolic", NULL, "temp", build_generic_math_tool },
    { "currency", "Currency Converter", "Offline exchange rates", "view-sort-ascending-symbolic", NULL, "currency", build_generic_math_tool },
    { "cooking", "Cooking Measure", "Cups · Spoons · Oz", "view-sort-ascending-symbolic", NULL, "cook", build_generic_math_tool },
    { "data_size", "Data Size Converter", "B · KB · MB · GB", "drive-harddisk-symbolic", NULL, "datasize", build_generic_math_tool },
    { "speed", "Speed Converter", "mph · km/h · m/s", "view-sort-ascending-symbolic", NULL, "speed", build_generic_math_tool },
    { "pressure", "Pressure Converter", "bar · psi · Pa", "view-sort-ascending-symbolic", NULL, "pressure", build_generic_math_tool },
    { "energy", "Energy Converter", "J · cal · kWh", "view-sort-ascending-symbolic", NULL, "energy", build_generic_math_tool },
    { .id = NULL }
};

static const HelvetiaTool tools_date[] = {
    { "date_diff", "Date Difference", "Calculate days between dates", "x-office-calendar-symbolic", NULL, "datediff", build_date_diff },
    { "age_calc", "Age Calculator", "Calculate exact age", "x-office-calendar-symbolic", NULL, "age", build_coming_soon },
    { "timezone", "Timezone Converter", "", "preferences-system-time-symbolic", NULL, "tz", build_timezone_studio },
    { "world_clock", "World Clock", "View multiple time zones", "preferences-system-time-symbolic", NULL, "clock", build_coming_soon },
    { "unix_ts", "Unix Timestamp", "Epoch to human readable", "preferences-system-time-symbolic", NULL, "epoch", build_unix_timestamp },
    { "cron_parse", "Cron Expression", "Parse crontab syntax", "preferences-system-time-symbolic", NULL, "cron", build_coming_soon },
    { "duration", "Duration Calculator", "Add/subtract time", "preferences-system-time-symbolic", NULL, "duration", build_coming_soon },
    { "week_num", "Week Number", "Find ISO week number", "x-office-calendar-symbolic", NULL, "week", build_coming_soon },
    { "timer", "Countdown Timer", "Simple timer", "preferences-system-time-symbolic", NULL, "timer", build_coming_soon },
    { "stopwatch", "Stopwatch", "Simple stopwatch", "preferences-system-time-symbolic", NULL, "stopwatch", build_stopwatch },
    { "pomodoro", "Pomodoro Timer", "Focus sessions", "preferences-system-time-symbolic", NULL, "pomodoro", build_coming_soon },
    { .id = NULL }
};

static const HelvetiaTool tools_color[] = {
    { "color_picker", "Color Picker", "Screen color grabber", "color-select-symbolic", NULL, "picker", build_coming_soon },
    { "color_convert", "Color Converter", "HEX ↔ RGB with preview", "color-select-symbolic", kw_color, "color", build_color_studio },
    { "palette_gen", "Palette Generator", "Generate complementary colors", "color-select-symbolic", NULL, "palette", build_coming_soon },
    { "contrast", "Contrast Checker", "WCAG AA / AAA score", "color-select-symbolic", NULL, "contrast", build_coming_soon },
    { "gradient", "Gradient Generator", "CSS gradient builder", "color-select-symbolic", NULL, "gradient", build_coming_soon },
    { "colorblind", "Color Blindness", "Simulate visual impairments", "color-select-symbolic", NULL, "colorblind", build_coming_soon },
    { .id = NULL }
};

static const HelvetiaTool tools_text[] = {
    { "case_convert", "Text Case Converter", "UPPER · lower · Title · sWAP", "format-text-direction-ltr-symbolic", kw_case, "case", build_case_tool },
    { "word_count", "Word & Char Counter", "Count words and characters", "format-text-direction-ltr-symbolic", NULL, "wc", build_word_counter },
    { "text_diff", "Text Diff", "Compare two texts", "format-text-direction-ltr-symbolic", NULL, "diff", build_text_diff },
    { "text_sort", "Text Sorter", "Sort lines alphabetically", "view-sort-ascending-symbolic", NULL, "sort", build_generic_text_tool },
    { "line_dedup", "Line Deduplicator", "Remove duplicate lines", "view-sort-ascending-symbolic", NULL, "dedup", build_generic_text_tool },
    { "find_replace", "Find & Replace", "Regex supported replace", "edit-find-replace-symbolic", NULL, "replace", build_generic_text_tool },
    { "regex_test", "Regex Tester", "Test regular expressions", "edit-find-symbolic", NULL, "regex", build_regex_tester },
    { "lorem", "Lorem Ipsum", "Generate placeholder text", "format-text-direction-ltr-symbolic", NULL, "lorem", build_lorem_ipsum },
    { "text_rev", "Text Reverser", "Reverse strings", "format-text-direction-rtl-symbolic", NULL, "reverse", build_generic_text_tool },
    { "trim", "Whitespace Trimmer", "Remove extra spaces", "format-text-direction-ltr-symbolic", NULL, "trim", build_generic_text_tool },
    { "slug", "Slug Generator", "URL-friendly strings", "format-text-direction-ltr-symbolic", NULL, "slug", build_generic_text_tool },
    { "md_preview", "Markdown Previewer", "Live markdown render", "text-html-symbolic", NULL, "md", build_generic_text_tool },
    { .id = NULL }
};

static const HelvetiaTool tools_gen[] = {
    { "pass_gen", "Password Generator", "Secure random passwords", "dialog-password-symbolic", NULL, "pass", build_password_generator },
    { "phrase_gen", "Passphrase Generator", "Diceware style passphrases", "dialog-password-symbolic", NULL, "phrase", build_generic_random_tool },
    { "uuid_gen", "UUID Generator", "Random Version 4 UUIDs", "text-x-generic-symbolic", kw_uuid, "uuid", build_uuid_tool },
    { "ulid_gen", "ULID Generator", "Lexicographically sortable", "text-x-generic-symbolic", NULL, "ulid", build_generic_random_tool },
    { "nanoid", "NanoID Generator", "Tiny unique IDs", "text-x-generic-symbolic", NULL, "nanoid", build_generic_random_tool },
    { "qr_gen", "QR Code Generator", "Text/URL to QR", "view-grid-symbolic", NULL, "qr", build_qr_studio },
    { "barcode", "Barcode Generator", "Generate 1D barcodes", "view-grid-symbolic", NULL, "barcode", build_generic_random_tool },
    { "hash_gen", "Hash Generator", "MD5 · SHA-1 · SHA-256 · SHA-512", "system-lock-screen-symbolic", kw_hash, "hash", build_hash_tool },
    { "hmac", "HMAC Generator", "Keyed-hash message auth", "system-lock-screen-symbolic", NULL, "hmac", build_generic_random_tool },
    { .id = NULL }
};

static const HelvetiaTool tools_everyday[] = {
    { "notes", "Notes / Scratchpad", "Quick temporary notes", "accessories-text-editor-symbolic", NULL, "notes", build_coming_soon },
    { "todo", "To-Do List", "Simple task list", "view-list-symbolic", NULL, "todo", build_coming_soon },
    { "clipboard", "Clipboard Manager", "View clipboard history", "edit-paste-symbolic", NULL, "clipboard", build_coming_soon },
    { "random_pick", "Random Picker", "Dice, coin, cards", "media-playlist-shuffle-symbolic", NULL, "pick", build_generic_random_tool },
    { "bmi", "BMI Calculator", "Body mass index", "accessories-calculator-symbolic", NULL, "bmi", build_coming_soon },
    { "tip_calc", "Tip Calculator", "Restaurant tip guide", "accessories-calculator-symbolic", NULL, "tipcalc", build_coming_soon },
    { .id = NULL }
};

static const HelvetiaSubcategory utility_subcategories[] = {
    { "Calculator & Math", tools_math },
    { "Statistics", tools_statistics },
    { "Geometry", tools_geometry },
    { "Units & Measures", tools_units },
    { "Date & Time", tools_date },
    { "Color", tools_color },
    { "Text", tools_text },
    { "Generators", tools_gen },
    { "Everyday", tools_everyday },
    { NULL, NULL }
};

/* ================================================================
 * Custom Dashboard Layout (Helvetia Design)
 * ================================================================ */

static void on_utility_card_clicked(GtkGestureClick *g, int n_press, double x, double y, gpointer data) {
    (void)g; (void)n_press; (void)x; (void)y;
    const HelvetiaTool *tool = data;
    GtkWidget *widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(g));
    GtkWidget *win = gtk_widget_get_ancestor(widget, HELVETIA_TYPE_WINDOW);
    if (win) {
        helvetia_window_open_tool(HELVETIA_WINDOW(win), tool);
    }
}

static GtkWidget *utility_create_view(void) {
    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_wide_handle(GTK_PANED(paned), FALSE);
    
    /* LEFT PANE: Calculator & Tape */
    GtkWidget *left_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start(left_box, 24);
    gtk_widget_set_margin_end(left_box, 24);
    gtk_widget_set_margin_top(left_box, 24);
    gtk_widget_set_margin_bottom(left_box, 24);
    
    GtkWidget *calc_header = gtk_label_new("Calculator & Tape");
    gtk_widget_add_css_class(calc_header, "helvetia-card-title");
    gtk_widget_set_halign(calc_header, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(left_box), calc_header);
    
    GtkWidget *calc_widget = build_calc_sci(); /* Using base converter as placeholder for the big calculator */
    gtk_box_append(GTK_BOX(left_box), calc_widget);
    
    gtk_paned_set_start_child(GTK_PANED(paned), left_box);
    
    /* RIGHT PANE: Installed Native Modules (12 Grid) */
    GtkWidget *right_scroll = gtk_scrolled_window_new();
    GtkWidget *right_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start(right_box, 24);
    gtk_widget_set_margin_end(right_box, 24);
    gtk_widget_set_margin_top(right_box, 24);
    gtk_widget_set_margin_bottom(right_box, 24);
    
    GtkWidget *modules_header = gtk_label_new("Installed Native Modules");
    gtk_widget_add_css_class(modules_header, "helvetia-card-title");
    gtk_widget_set_halign(modules_header, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(right_box), modules_header);
    
    /* Flowbox for tools */
    GtkWidget *flowbox = gtk_flow_box_new();
    gtk_widget_set_valign(flowbox, GTK_ALIGN_START);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(flowbox), 2);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flowbox), 3);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flowbox), GTK_SELECTION_NONE);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(flowbox), 16);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(flowbox), 16);
    
    /* Add the 12 tools (Mocked for now) */
    const HelvetiaTool *tools[] = {
        &tools_math[0],
        &tools_units[0],
        &tools_color[1],
        &tools_gen[7],
        &tools_gen[5],
        &tools_gen[0],
        &tools_math[7],
        &tools_text[0],
        &tools_gen[2],
        &tools_date[2],
        &tools_text[7],
        &tools_math[1]
    };
    
    for (int i = 0; i < 12; i++) {
        GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
        gtk_widget_add_css_class(card, "helvetia-card");
        
        GtkWidget *title = gtk_label_new(tools[i]->name);
        gtk_widget_add_css_class(title, "helvetia-card-title");
        gtk_widget_set_halign(title, GTK_ALIGN_START);
        
        GtkWidget *desc = gtk_label_new(tools[i]->description);
        gtk_widget_add_css_class(desc, "helvetia-card-subtitle");
        gtk_widget_set_halign(desc, GTK_ALIGN_START);
        
        gtk_box_append(GTK_BOX(card), title);
        gtk_box_append(GTK_BOX(card), desc);
        
        GtkGesture *click = gtk_gesture_click_new();
        g_signal_connect(click, "pressed", G_CALLBACK(on_utility_card_clicked), (gpointer)tools[i]);
        gtk_widget_add_controller(card, GTK_EVENT_CONTROLLER(click));
        gtk_widget_set_cursor_from_name(card, "pointer");
        
        gtk_flow_box_insert(GTK_FLOW_BOX(flowbox), card, -1);
    }
    
    gtk_box_append(GTK_BOX(right_box), flowbox);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(right_scroll), right_box);
    gtk_paned_set_end_child(GTK_PANED(paned), right_scroll);
    
    gtk_paned_set_position(GTK_PANED(paned), 360); /* 360px left pane */
    
    return paned;
}

/* ================================================================
 * Module descriptor & registration
 * ================================================================ */
static const HelvetiaModule utility_module = {
    .id            = "utility",
    .name          = "Utilities",
    .icon_name     = "applications-utilities",
    .description   = "Calculators, generators, converters, and quick-reference tools — all offline.",
    .subcategories = utility_subcategories,
    .create_view   = utility_create_view,
    .on_activate   = NULL,
    .on_deactivate = NULL,
    .on_shutdown   = NULL,
};

const HelvetiaModule *helvetia_utility_get_module(void) {
    return &utility_module;
}
