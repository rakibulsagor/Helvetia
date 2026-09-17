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

    /* Each drop-down MUST have its own model — sharing one model between
     * two GtkDropDown widgets causes a GTK list-item-manager assertion. */
    GtkStringList *from_model = gtk_string_list_new(list);
    GtkStringList *to_model   = gtk_string_list_new(list);

    gtk_drop_down_set_model(GTK_DROP_DOWN(ctx->from_dd), G_LIST_MODEL(from_model));
    gtk_drop_down_set_model(GTK_DROP_DOWN(ctx->to_dd),   G_LIST_MODEL(to_model));

    g_object_unref(from_model);
    g_object_unref(to_model);
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
    g_object_unref(from_model); /* drop-down took its own ref */
    g_object_unref(to_model);
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

    g_signal_connect(cat_dd, "notify::selected",
                     G_CALLBACK(on_unit_category_changed), ctx);
    g_signal_connect_data(btn, "clicked", G_CALLBACK(on_unit_convert),
                          ctx, closure_free, 0);
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
 * Tool 6 — Color Tool  (HEX ↔ RGB)
 * ================================================================ */
typedef struct { GtkWidget *hex_entry, *r_entry, *g_entry, *b_entry, *swatch; } ColorCtx;

static void update_swatch(ColorCtx *ctx, guint8 r, guint8 g, guint8 b) {
    (void)ctx;
    char css[128];
    snprintf(css, sizeof css,
        ".helvetia-swatch { background-color: rgb(%u,%u,%u); border-radius:8px; min-height:50px; }",
        r, g, b);
    GtkCssProvider *p = gtk_css_provider_new();
    gtk_css_provider_load_from_string(p, css);
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(p),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);
    g_object_unref(p);
}

static void on_hex_to_rgb(GtkButton *btn, gpointer user_data) {
    (void)btn;
    ColorCtx *ctx  = user_data;
    const char *hex = gtk_editable_get_text(GTK_EDITABLE(ctx->hex_entry));
    /* strip leading # */
    if (*hex == '#') hex++;
    if (strlen(hex) != 6) { return; }

    unsigned int r, g, b;
    if (sscanf(hex, "%02x%02x%02x", &r, &g, &b) != 3) return;

    char sr[4], sg[4], sb[4];
    snprintf(sr,sizeof sr,"%u",r);
    snprintf(sg,sizeof sg,"%u",g);
    snprintf(sb,sizeof sb,"%u",b);
    gtk_editable_set_text(GTK_EDITABLE(ctx->r_entry), sr);
    gtk_editable_set_text(GTK_EDITABLE(ctx->g_entry), sg);
    gtk_editable_set_text(GTK_EDITABLE(ctx->b_entry), sb);
    update_swatch(ctx, (guint8)r, (guint8)g, (guint8)b);
}

static void on_rgb_to_hex(GtkButton *btn, gpointer user_data) {
    (void)btn;
    ColorCtx *ctx = user_data;
    int r = atoi(gtk_editable_get_text(GTK_EDITABLE(ctx->r_entry)));
    int g = atoi(gtk_editable_get_text(GTK_EDITABLE(ctx->g_entry)));
    int b = atoi(gtk_editable_get_text(GTK_EDITABLE(ctx->b_entry)));
    r = CLAMP(r, 0, 255); g = CLAMP(g, 0, 255); b = CLAMP(b, 0, 255);
    char buf[8];
    snprintf(buf, sizeof buf, "#%02X%02X%02X", r, g, b);
    gtk_editable_set_text(GTK_EDITABLE(ctx->hex_entry), buf);
    update_swatch(ctx, (guint8)r, (guint8)g, (guint8)b);
}

static GtkWidget *build_color_tool(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    GtkWidget *hex_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(hex_entry), "#RRGGBB");

    GtkWidget *rgb_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *r_e = gtk_entry_new(), *g_e = gtk_entry_new(), *b_e = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(r_e), "R");
    gtk_entry_set_placeholder_text(GTK_ENTRY(g_e), "G");
    gtk_entry_set_placeholder_text(GTK_ENTRY(b_e), "B");
    gtk_widget_set_hexpand(r_e, TRUE);
    gtk_widget_set_hexpand(g_e, TRUE);
    gtk_widget_set_hexpand(b_e, TRUE);
    gtk_box_append(GTK_BOX(rgb_row), r_e);
    gtk_box_append(GTK_BOX(rgb_row), g_e);
    gtk_box_append(GTK_BOX(rgb_row), b_e);

    GtkWidget *swatch = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(swatch, "helvetia-swatch");

    ColorCtx *ctx   = g_new0(ColorCtx, 1);
    ctx->hex_entry  = hex_entry;
    ctx->r_entry    = r_e;
    ctx->g_entry    = g_e;
    ctx->b_entry    = b_e;
    ctx->swatch     = swatch;

    GtkWidget *btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *h2r = gtk_button_new_with_label("HEX → RGB");
    GtkWidget *r2h = gtk_button_new_with_label("RGB → HEX");
    gtk_widget_add_css_class(h2r, "suggested-action");
    gtk_widget_add_css_class(r2h, "suggested-action");

    g_signal_connect_data(h2r, "clicked", G_CALLBACK(on_hex_to_rgb),
                          ctx, closure_free, 0);
    g_signal_connect_data(r2h, "clicked", G_CALLBACK(on_rgb_to_hex),
                          ctx, NULL, 0);

    gtk_box_append(GTK_BOX(btn_row), h2r);
    gtk_box_append(GTK_BOX(btn_row), r2h);

    gtk_box_append(GTK_BOX(box), hex_entry);
    gtk_box_append(GTK_BOX(box), gtk_label_new("— or —"));
    gtk_box_append(GTK_BOX(box), rgb_row);
    gtk_box_append(GTK_BOX(box), btn_row);
    gtk_box_append(GTK_BOX(box), swatch);

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
    { "calc_sci", "Scientific Calculator", "Advanced math operations", "accessories-calculator-symbolic", NULL, "calc", NULL },
    { "calc_pct", "Percentage Calculator", "Quick percentage math", "accessories-calculator-symbolic", NULL, "pct", NULL },
    { "calc_disc", "Discount Calculator", "Calculate final price", "accessories-calculator-symbolic", NULL, "discount", NULL },
    { "calc_tip", "Tip Splitter", "Split bills easily", "accessories-calculator-symbolic", NULL, "tip", NULL },
    { "calc_markup", "Markup Calculator", "Calculate profit margins", "accessories-calculator-symbolic", NULL, "markup", NULL },
    { "calc_loan", "Loan / EMI Calculator", "Calculate loan payments", "accessories-calculator-symbolic", NULL, "loan", NULL },
    { "calc_compound", "Compound Interest", "Calculate investments", "accessories-calculator-symbolic", NULL, "interest", NULL },
    { "base_convert", "Number Base Converter", "Bin · Oct · Dec · Hex", "format-text-numeric-symbolic", kw_base, "base", build_base_converter },
    { "roman_num", "Roman Numeral", "Convert to/from Roman numerals", "format-text-numeric-symbolic", NULL, "roman", NULL },
    { "rand_num", "Random Number", "Generate random integers", "media-playlist-shuffle-symbolic", NULL, "rand", NULL },
    { .id = NULL }
};

static const HelvetiaTool tools_units[] = {
    { "unit_convert", "Unit Converter", "Length · Weight · Temperature", "view-sort-ascending-symbolic", kw_unit, "unit", build_unit_converter },
    { "temp_convert", "Temperature Converter", "C · F · K", "view-sort-ascending-symbolic", NULL, "temp", NULL },
    { "currency", "Currency Converter", "Offline exchange rates", "view-sort-ascending-symbolic", NULL, "currency", NULL },
    { "cooking", "Cooking Measure", "Cups · Spoons · Oz", "view-sort-ascending-symbolic", NULL, "cook", NULL },
    { "data_size", "Data Size Converter", "B · KB · MB · GB", "drive-harddisk-symbolic", NULL, "datasize", NULL },
    { "speed", "Speed Converter", "mph · km/h · m/s", "view-sort-ascending-symbolic", NULL, "speed", NULL },
    { "pressure", "Pressure Converter", "bar · psi · Pa", "view-sort-ascending-symbolic", NULL, "pressure", NULL },
    { "energy", "Energy Converter", "J · cal · kWh", "view-sort-ascending-symbolic", NULL, "energy", NULL },
    { .id = NULL }
};

static const HelvetiaTool tools_date[] = {
    { "date_diff", "Date Difference", "Calculate days between dates", "x-office-calendar-symbolic", NULL, "datediff", NULL },
    { "age_calc", "Age Calculator", "Calculate exact age", "x-office-calendar-symbolic", NULL, "age", NULL },
    { "timezone", "Timezone Converter", "Compare time zones", "preferences-system-time-symbolic", NULL, "tz", NULL },
    { "world_clock", "World Clock", "View multiple time zones", "preferences-system-time-symbolic", NULL, "clock", NULL },
    { "unix_ts", "Unix Timestamp", "Epoch to human readable", "preferences-system-time-symbolic", NULL, "epoch", NULL },
    { "cron_parse", "Cron Expression", "Parse crontab syntax", "preferences-system-time-symbolic", NULL, "cron", NULL },
    { "duration", "Duration Calculator", "Add/subtract time", "preferences-system-time-symbolic", NULL, "duration", NULL },
    { "week_num", "Week Number", "Find ISO week number", "x-office-calendar-symbolic", NULL, "week", NULL },
    { "timer", "Countdown Timer", "Simple timer", "preferences-system-time-symbolic", NULL, "timer", NULL },
    { "stopwatch", "Stopwatch", "Simple stopwatch", "preferences-system-time-symbolic", NULL, "stopwatch", NULL },
    { "pomodoro", "Pomodoro Timer", "Focus sessions", "preferences-system-time-symbolic", NULL, "pomodoro", NULL },
    { .id = NULL }
};

static const HelvetiaTool tools_color[] = {
    { "color_picker", "Color Picker", "Screen color grabber", "color-select-symbolic", NULL, "picker", NULL },
    { "color_convert", "Color Converter", "HEX ↔ RGB with preview", "color-select-symbolic", kw_color, "color", build_color_tool },
    { "palette_gen", "Palette Generator", "Generate complementary colors", "color-select-symbolic", NULL, "palette", NULL },
    { "contrast", "Contrast Checker", "WCAG AA / AAA score", "color-select-symbolic", NULL, "contrast", NULL },
    { "gradient", "Gradient Generator", "CSS gradient builder", "color-select-symbolic", NULL, "gradient", NULL },
    { "colorblind", "Color Blindness", "Simulate visual impairments", "color-select-symbolic", NULL, "colorblind", NULL },
    { .id = NULL }
};

static const HelvetiaTool tools_text[] = {
    { "case_convert", "Text Case Converter", "UPPER · lower · Title · sWAP", "format-text-direction-ltr-symbolic", kw_case, "case", build_case_tool },
    { "word_count", "Word & Char Counter", "Count words and characters", "format-text-direction-ltr-symbolic", NULL, "wc", NULL },
    { "text_diff", "Text Diff", "Compare two texts", "format-text-direction-ltr-symbolic", NULL, "diff", NULL },
    { "text_sort", "Text Sorter", "Sort lines alphabetically", "view-sort-ascending-symbolic", NULL, "sort", NULL },
    { "line_dedup", "Line Deduplicator", "Remove duplicate lines", "view-sort-ascending-symbolic", NULL, "dedup", NULL },
    { "find_replace", "Find & Replace", "Regex supported replace", "edit-find-replace-symbolic", NULL, "replace", NULL },
    { "regex_test", "Regex Tester", "Test regular expressions", "edit-find-symbolic", NULL, "regex", NULL },
    { "lorem", "Lorem Ipsum", "Generate placeholder text", "format-text-direction-ltr-symbolic", NULL, "lorem", NULL },
    { "text_rev", "Text Reverser", "Reverse strings", "format-text-direction-rtl-symbolic", NULL, "reverse", NULL },
    { "trim", "Whitespace Trimmer", "Remove extra spaces", "format-text-direction-ltr-symbolic", NULL, "trim", NULL },
    { "slug", "Slug Generator", "URL-friendly strings", "format-text-direction-ltr-symbolic", NULL, "slug", NULL },
    { "md_preview", "Markdown Previewer", "Live markdown render", "text-html-symbolic", NULL, "md", NULL },
    { .id = NULL }
};

static const HelvetiaTool tools_gen[] = {
    { "pass_gen", "Password Generator", "Secure random passwords", "dialog-password-symbolic", NULL, "pass", NULL },
    { "phrase_gen", "Passphrase Generator", "Diceware style passphrases", "dialog-password-symbolic", NULL, "phrase", NULL },
    { "uuid_gen", "UUID Generator", "Random Version 4 UUIDs", "text-x-generic-symbolic", kw_uuid, "uuid", build_uuid_tool },
    { "ulid_gen", "ULID Generator", "Lexicographically sortable", "text-x-generic-symbolic", NULL, "ulid", NULL },
    { "nanoid", "NanoID Generator", "Tiny unique IDs", "text-x-generic-symbolic", NULL, "nanoid", NULL },
    { "qr_gen", "QR Code Generator", "Text/URL to QR", "view-grid-symbolic", NULL, "qr", NULL },
    { "barcode", "Barcode Generator", "Generate 1D barcodes", "view-grid-symbolic", NULL, "barcode", NULL },
    { "hash_gen", "Hash Generator", "MD5 · SHA-1 · SHA-256 · SHA-512", "system-lock-screen-symbolic", kw_hash, "hash", build_hash_tool },
    { "hmac", "HMAC Generator", "Keyed-hash message auth", "system-lock-screen-symbolic", NULL, "hmac", NULL },
    { .id = NULL }
};

static const HelvetiaTool tools_everyday[] = {
    { "notes", "Notes / Scratchpad", "Quick temporary notes", "accessories-text-editor-symbolic", NULL, "notes", NULL },
    { "todo", "To-Do List", "Simple task list", "view-list-symbolic", NULL, "todo", NULL },
    { "clipboard", "Clipboard Manager", "View clipboard history", "edit-paste-symbolic", NULL, "clipboard", NULL },
    { "random_pick", "Random Picker", "Dice, coin, cards", "media-playlist-shuffle-symbolic", NULL, "pick", NULL },
    { "bmi", "BMI Calculator", "Body mass index", "accessories-calculator-symbolic", NULL, "bmi", NULL },
    { "tip_calc", "Tip Calculator", "Restaurant tip guide", "accessories-calculator-symbolic", NULL, "tipcalc", NULL },
    { .id = NULL }
};

static const HelvetiaSubcategory utility_subcategories[] = {
    { "Calculator & Math", tools_math },
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
    
    GtkWidget *calc_widget = build_base_converter(); /* Using base converter as placeholder for the big calculator */
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
