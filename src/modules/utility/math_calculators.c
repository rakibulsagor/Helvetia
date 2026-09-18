#include "utility_module.h"
#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

// ---------------------------------------------------------
// Standard Deviation Calculator
// ---------------------------------------------------------
typedef struct {
    GtkWidget *text_view;
    GtkWidget *radio_pop;
    GtkWidget *lbl_stddev;
    GtkWidget *lbl_var;
    GtkWidget *lbl_mean;
    GtkWidget *lbl_sum;
    GtkWidget *lbl_moe;
} StddevCtx;

static void on_stddev_calc(GtkButton *btn, gpointer data) {
    (void)btn;
    StddevCtx *ctx = data;
    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctx->text_view));
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buf, &start, &end);
    char *text = gtk_text_buffer_get_text(buf, &start, &end, FALSE);
    
    // Parse comma or space separated values
    double values[1000];
    int count = 0;
    
    char *token = strtok(text, " ,\n\t");
    while (token && count < 1000) {
        values[count++] = atof(token);
        token = strtok(NULL, " ,\n\t");
    }
    g_free(text);
    
    if (count == 0) return;
    
    double sum = 0;
    for (int i=0; i<count; i++) sum += values[i];
    double mean = sum / count;
    
    double variance_sum = 0;
    for (int i=0; i<count; i++) {
        double diff = values[i] - mean;
        variance_sum += diff * diff;
    }
    
    gboolean is_pop = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctx->radio_pop));
    int denom = is_pop ? count : (count > 1 ? count - 1 : 1);
    
    double variance = variance_sum / denom;
    double stddev = sqrt(variance);
    double moe = 1.96 * stddev / sqrt(count);
    
    char out[128];
    snprintf(out, sizeof out, "%.6f", stddev);
    gtk_label_set_text(GTK_LABEL(ctx->lbl_stddev), out);
    
    snprintf(out, sizeof out, "%.6f", variance);
    gtk_label_set_text(GTK_LABEL(ctx->lbl_var), out);
    
    snprintf(out, sizeof out, "%.6f", mean);
    gtk_label_set_text(GTK_LABEL(ctx->lbl_mean), out);
    
    snprintf(out, sizeof out, "%.6f", sum);
    gtk_label_set_text(GTK_LABEL(ctx->lbl_sum), out);
    
    snprintf(out, sizeof out, "%.6f", moe);
    gtk_label_set_text(GTK_LABEL(ctx->lbl_moe), out);
}

static void on_stddev_clear(GtkButton *btn, gpointer data) {
    (void)btn;
    StddevCtx *ctx = data;
    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctx->text_view));
    gtk_text_buffer_set_text(buf, "", -1);
    gtk_label_set_text(GTK_LABEL(ctx->lbl_stddev), "-");
    gtk_label_set_text(GTK_LABEL(ctx->lbl_var), "-");
    gtk_label_set_text(GTK_LABEL(ctx->lbl_mean), "-");
    gtk_label_set_text(GTK_LABEL(ctx->lbl_sum), "-");
    gtk_label_set_text(GTK_LABEL(ctx->lbl_moe), "-");
}

static void stddev_add_row(GtkWidget *grid, int row, const char *title, GtkWidget *lbl) {
    GtkWidget *tl = gtk_label_new(title);
    gtk_widget_set_halign(tl, GTK_ALIGN_START);
    gtk_widget_add_css_class(tl, "dim-label");
    gtk_widget_set_halign(lbl, GTK_ALIGN_END);
    gtk_widget_add_css_class(lbl, "helvetia-title");
    gtk_grid_attach(GTK_GRID(grid), tl, 0, row, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), lbl, 1, row, 1, 1);
}

GtkWidget *build_stddev_calc(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    StddevCtx *ctx = g_new0(StddevCtx, 1);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);
    
    ctx->text_view = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(ctx->text_view), GTK_WRAP_WORD_CHAR);
    gtk_widget_set_size_request(ctx->text_view, -1, 100);
    gtk_widget_add_css_class(ctx->text_view, "helvetia-result");
    
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), ctx->text_view);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scroll), 100);
    gtk_box_append(GTK_BOX(box), scroll);
    
    GtkWidget *hbox_radio = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(hbox_radio, GTK_ALIGN_CENTER);
    ctx->radio_pop = gtk_toggle_button_new_with_label("Population");
    GtkWidget *radio_samp = gtk_toggle_button_new_with_label("Sample");
    gtk_toggle_button_set_group(GTK_TOGGLE_BUTTON(radio_samp), GTK_TOGGLE_BUTTON(ctx->radio_pop));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctx->radio_pop), TRUE);
    gtk_box_append(GTK_BOX(hbox_radio), gtk_label_new("It is a:"));
    gtk_box_append(GTK_BOX(hbox_radio), ctx->radio_pop);
    gtk_box_append(GTK_BOX(hbox_radio), radio_samp);
    gtk_box_append(GTK_BOX(box), hbox_radio);
    
    GtkWidget *hbox_btns = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(hbox_btns, GTK_ALIGN_CENTER);
    GtkWidget *btn_calc = gtk_button_new_with_label("Calculate");
    gtk_widget_add_css_class(btn_calc, "suggested-action");
    g_signal_connect(btn_calc, "clicked", G_CALLBACK(on_stddev_calc), ctx);
    GtkWidget *btn_clear = gtk_button_new_with_label("Clear");
    g_signal_connect(btn_clear, "clicked", G_CALLBACK(on_stddev_clear), ctx);
    gtk_box_append(GTK_BOX(hbox_btns), btn_calc);
    gtk_box_append(GTK_BOX(hbox_btns), btn_clear);
    gtk_box_append(GTK_BOX(box), hbox_btns);
    
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_widget_set_margin_top(grid, 20);
    gtk_widget_set_halign(grid, GTK_ALIGN_CENTER);
    
    ctx->lbl_stddev = gtk_label_new("-");
    ctx->lbl_var = gtk_label_new("-");
    ctx->lbl_mean = gtk_label_new("-");
    ctx->lbl_sum = gtk_label_new("-");
    ctx->lbl_moe = gtk_label_new("-");
    
    // Helper to format labels
    
    stddev_add_row(grid, 0, "Standard Deviation:", ctx->lbl_stddev);
    stddev_add_row(grid, 1, "Variance:", ctx->lbl_var);
    stddev_add_row(grid, 2, "Mean:", ctx->lbl_mean);
    stddev_add_row(grid, 3, "Sum:", ctx->lbl_sum);
    stddev_add_row(grid, 4, "Margin of Error (95%):", ctx->lbl_moe);
    
    gtk_box_append(GTK_BOX(box), grid);
    return box;
}

// ---------------------------------------------------------
// Percentage Calculator
// ---------------------------------------------------------
typedef struct {
    GtkWidget *e1_pct; GtkWidget *e1_base; GtkWidget *l1_res;
    GtkWidget *e2_val; GtkWidget *e2_base; GtkWidget *l2_res;
    GtkWidget *e3_val; GtkWidget *e3_pct;  GtkWidget *l3_res;
} PctCtx;

static void on_pct_calc1(GtkButton *btn, gpointer data) {
    (void)btn;
    PctCtx *ctx = data;
    double p = atof(gtk_editable_get_text(GTK_EDITABLE(ctx->e1_pct)));
    double b = atof(gtk_editable_get_text(GTK_EDITABLE(ctx->e1_base)));
    char buf[64];
    snprintf(buf, sizeof buf, "%.4f", (p / 100.0) * b);
    gtk_label_set_text(GTK_LABEL(ctx->l1_res), buf);
}
static void on_pct_calc2(GtkButton *btn, gpointer data) {
    (void)btn;
    PctCtx *ctx = data;
    double v = atof(gtk_editable_get_text(GTK_EDITABLE(ctx->e2_val)));
    double b = atof(gtk_editable_get_text(GTK_EDITABLE(ctx->e2_base)));
    char buf[64];
    if (b == 0) strcpy(buf, "Div by 0");
    else snprintf(buf, sizeof buf, "%.4f %%", (v / b) * 100.0);
    gtk_label_set_text(GTK_LABEL(ctx->l2_res), buf);
}
static void on_pct_calc3(GtkButton *btn, gpointer data) {
    (void)btn;
    PctCtx *ctx = data;
    double v = atof(gtk_editable_get_text(GTK_EDITABLE(ctx->e3_val)));
    double p = atof(gtk_editable_get_text(GTK_EDITABLE(ctx->e3_pct)));
    char buf[64];
    if (p == 0) strcpy(buf, "Div by 0");
    else snprintf(buf, sizeof buf, "%.4f", v / (p / 100.0));
    gtk_label_set_text(GTK_LABEL(ctx->l3_res), buf);
}

static GtkWidget* pct_add_row(GtkWidget *parent, PctCtx *ctx, const char *t1, GtkWidget **e1, const char *t2, GtkWidget **e2, const char *t3, void (*cb)(GtkButton*, gpointer), GtkWidget **lout) {
    GtkWidget *hb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(hb, GTK_ALIGN_CENTER);
    
    gtk_box_append(GTK_BOX(hb), gtk_label_new(t1));
    *e1 = gtk_entry_new();
    gtk_widget_set_size_request(*e1, 100, -1);
    gtk_box_append(GTK_BOX(hb), *e1);
    
    gtk_box_append(GTK_BOX(hb), gtk_label_new(t2));
    *e2 = gtk_entry_new();
    gtk_widget_set_size_request(*e2, 100, -1);
    gtk_box_append(GTK_BOX(hb), *e2);
    
    gtk_box_append(GTK_BOX(hb), gtk_label_new(t3));
    GtkWidget *btn = gtk_button_new_with_label("Calculate");
    gtk_widget_add_css_class(btn, "suggested-action");
    g_signal_connect(btn, "clicked", G_CALLBACK(cb), ctx);
    gtk_box_append(GTK_BOX(hb), btn);
    
    *lout = gtk_label_new("-");
    gtk_widget_add_css_class(*lout, "helvetia-title");
    gtk_box_append(GTK_BOX(hb), *lout);
    
    gtk_box_append(GTK_BOX(parent), hb);
    return hb;
}

GtkWidget *build_pct_calc(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 24);
    PctCtx *ctx = g_new0(PctCtx, 1);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);
    
    // Helper
    pct_add_row(box, ctx, "What is", &ctx->e1_pct, "% of", &ctx->e1_base, "? =", on_pct_calc1, &ctx->l1_res);
    pct_add_row(box, ctx, "", &ctx->e2_val, "is what % of", &ctx->e2_base, "? =", on_pct_calc2, &ctx->l2_res);
    pct_add_row(box, ctx, "", &ctx->e3_val, "is", &ctx->e3_pct, "% of what ? =", on_pct_calc3, &ctx->l3_res);
    
    return box;
}

// ---------------------------------------------------------
// Volume Calculator
// ---------------------------------------------------------
typedef struct {
    GtkWidget *stack;
    
    // Sphere
    GtkWidget *e_sph_r; GtkWidget *l_sph_res;
    // Cylinder
    GtkWidget *e_cyl_r; GtkWidget *e_cyl_h; GtkWidget *l_cyl_res;
    // Cone
    GtkWidget *e_con_r; GtkWidget *e_con_h; GtkWidget *l_con_res;
    // Cube
    GtkWidget *e_cub_a; GtkWidget *l_cub_res;
    // Rect
    GtkWidget *e_rec_l; GtkWidget *e_rec_w; GtkWidget *e_rec_h; GtkWidget *l_rec_res;
} VolCtx;

static void on_vol_sph(GtkButton *btn, gpointer data) {
    (void)btn;
    VolCtx *ctx = data;
    double r = atof(gtk_editable_get_text(GTK_EDITABLE(ctx->e_sph_r)));
    char buf[64]; snprintf(buf, sizeof buf, "%.4f", (4.0/3.0) * G_PI * pow(r, 3));
    gtk_label_set_text(GTK_LABEL(ctx->l_sph_res), buf);
}
static void on_vol_cyl(GtkButton *btn, gpointer data) {
    (void)btn;
    VolCtx *ctx = data;
    double r = atof(gtk_editable_get_text(GTK_EDITABLE(ctx->e_cyl_r)));
    double h = atof(gtk_editable_get_text(GTK_EDITABLE(ctx->e_cyl_h)));
    char buf[64]; snprintf(buf, sizeof buf, "%.4f", G_PI * r * r * h);
    gtk_label_set_text(GTK_LABEL(ctx->l_cyl_res), buf);
}
static void on_vol_con(GtkButton *btn, gpointer data) {
    (void)btn;
    VolCtx *ctx = data;
    double r = atof(gtk_editable_get_text(GTK_EDITABLE(ctx->e_con_r)));
    double h = atof(gtk_editable_get_text(GTK_EDITABLE(ctx->e_con_h)));
    char buf[64]; snprintf(buf, sizeof buf, "%.4f", G_PI * r * r * (h / 3.0));
    gtk_label_set_text(GTK_LABEL(ctx->l_con_res), buf);
}
static void on_vol_cub(GtkButton *btn, gpointer data) {
    (void)btn;
    VolCtx *ctx = data;
    double a = atof(gtk_editable_get_text(GTK_EDITABLE(ctx->e_cub_a)));
    char buf[64]; snprintf(buf, sizeof buf, "%.4f", pow(a, 3));
    gtk_label_set_text(GTK_LABEL(ctx->l_cub_res), buf);
}
static void on_vol_rec(GtkButton *btn, gpointer data) {
    (void)btn;
    VolCtx *ctx = data;
    double l = atof(gtk_editable_get_text(GTK_EDITABLE(ctx->e_rec_l)));
    double w = atof(gtk_editable_get_text(GTK_EDITABLE(ctx->e_rec_w)));
    double h = atof(gtk_editable_get_text(GTK_EDITABLE(ctx->e_rec_h)));
    char buf[64]; snprintf(buf, sizeof buf, "%.4f", l * w * h);
    gtk_label_set_text(GTK_LABEL(ctx->l_rec_res), buf);
}

static GtkWidget* vol_add_page(VolCtx *ctx, const char *title, GtkWidget **e1, const char *l1, GtkWidget **e2, const char *l2, GtkWidget **e3, const char *l3, void (*cb)(GtkButton*, gpointer), GtkWidget **lout) {
    GtkWidget *b = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_top(b, 20); gtk_widget_set_margin_bottom(b, 20);
    
    GtkWidget *g = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(g), 8); gtk_grid_set_column_spacing(GTK_GRID(g), 8);
    gtk_widget_set_halign(g, GTK_ALIGN_CENTER);
    
    int r = 0;
    if (e1) { gtk_grid_attach(GTK_GRID(g), gtk_label_new(l1), 0, r, 1, 1); *e1 = gtk_entry_new(); gtk_grid_attach(GTK_GRID(g), *e1, 1, r++, 1, 1); }
    if (e2) { gtk_grid_attach(GTK_GRID(g), gtk_label_new(l2), 0, r, 1, 1); *e2 = gtk_entry_new(); gtk_grid_attach(GTK_GRID(g), *e2, 1, r++, 1, 1); }
    if (e3) { gtk_grid_attach(GTK_GRID(g), gtk_label_new(l3), 0, r, 1, 1); *e3 = gtk_entry_new(); gtk_grid_attach(GTK_GRID(g), *e3, 1, r++, 1, 1); }
    
    GtkWidget *btn = gtk_button_new_with_label("Calculate");
    gtk_widget_add_css_class(btn, "suggested-action");
    g_signal_connect(btn, "clicked", G_CALLBACK(cb), ctx);
    gtk_grid_attach(GTK_GRID(g), btn, 0, r, 2, 1);
    
    *lout = gtk_label_new("-");
    gtk_widget_add_css_class(*lout, "helvetia-title");
    gtk_grid_attach(GTK_GRID(g), gtk_label_new("Volume:"), 0, r+1, 1, 1);
    gtk_grid_attach(GTK_GRID(g), *lout, 1, r+1, 1, 1);
    
    gtk_box_append(GTK_BOX(b), g);
    gtk_stack_add_titled(GTK_STACK(ctx->stack), b, title, title);
    return b;
}

GtkWidget *build_vol_calc(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    VolCtx *ctx = g_new0(VolCtx, 1);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);
    
    ctx->stack = gtk_stack_new();
    GtkWidget *switcher = gtk_stack_switcher_new();
    gtk_stack_switcher_set_stack(GTK_STACK_SWITCHER(switcher), GTK_STACK(ctx->stack));
    gtk_widget_set_halign(switcher, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(box), switcher);
    
    vol_add_page(ctx, "Sphere", &ctx->e_sph_r, "Radius (r):", NULL, NULL, NULL, NULL, on_vol_sph, &ctx->l_sph_res);
    vol_add_page(ctx, "Cylinder", &ctx->e_cyl_r, "Radius (r):", &ctx->e_cyl_h, "Height (h):", NULL, NULL, on_vol_cyl, &ctx->l_cyl_res);
    vol_add_page(ctx, "Cone", &ctx->e_con_r, "Radius (r):", &ctx->e_con_h, "Height (h):", NULL, NULL, on_vol_con, &ctx->l_con_res);
    vol_add_page(ctx, "Cube", &ctx->e_cub_a, "Edge (a):", NULL, NULL, NULL, NULL, on_vol_cub, &ctx->l_cub_res);
    vol_add_page(ctx, "Rectangular", &ctx->e_rec_l, "Length (l):", &ctx->e_rec_w, "Width (w):", &ctx->e_rec_h, "Height (h):", on_vol_rec, &ctx->l_rec_res);
    
    gtk_box_append(GTK_BOX(box), ctx->stack);
    return box;
}

// ---------------------------------------------------------
// Fraction Calculator
// ---------------------------------------------------------
typedef struct {
    GtkWidget *e_num1; GtkWidget *e_den1;
    GtkWidget *e_num2; GtkWidget *e_den2;
    GtkWidget *cb_op;
    GtkWidget *l_res_frac;
    GtkWidget *l_res_mixed;
    GtkWidget *l_res_dec;
} FracCtx;

static long long gcd(long long a, long long b) {
    a = llabs(a); b = llabs(b);
    while (b != 0) { long long t = b; b = a % b; a = t; }
    return a;
}

static void on_frac_calc(GtkButton *btn, gpointer data) {
    (void)btn;
    FracCtx *ctx = data;
    long long n1 = atoll(gtk_editable_get_text(GTK_EDITABLE(ctx->e_num1)));
    long long d1 = atoll(gtk_editable_get_text(GTK_EDITABLE(ctx->e_den1)));
    long long n2 = atoll(gtk_editable_get_text(GTK_EDITABLE(ctx->e_num2)));
    long long d2 = atoll(gtk_editable_get_text(GTK_EDITABLE(ctx->e_den2)));
    
    if (d1 == 0 || d2 == 0) {
        gtk_label_set_text(GTK_LABEL(ctx->l_res_frac), "Div by 0"); return;
    }
    
    int op = gtk_drop_down_get_selected(GTK_DROP_DOWN(ctx->cb_op)); // 0:+, 1:-, 2:*, 3:/
    long long nr = 0, dr = 1;
    if (op == 0) { nr = n1*d2 + n2*d1; dr = d1*d2; }
    else if (op == 1) { nr = n1*d2 - n2*d1; dr = d1*d2; }
    else if (op == 2) { nr = n1*n2; dr = d1*d2; }
    else if (op == 3) { nr = n1*d2; dr = d1*n2; }
    
    if (dr == 0) { gtk_label_set_text(GTK_LABEL(ctx->l_res_frac), "Div by 0"); return; }
    if (dr < 0) { nr = -nr; dr = -dr; }
    
    long long g = gcd(nr, dr);
    nr /= g; dr /= g;
    
    char buf[128];
    snprintf(buf, sizeof buf, "%lld / %lld", nr, dr);
    gtk_label_set_text(GTK_LABEL(ctx->l_res_frac), buf);
    
    if (llabs(nr) >= dr && dr != 1) {
        long long whole = nr / dr;
        long long rem = llabs(nr % dr);
        snprintf(buf, sizeof buf, "%lld  %lld/%lld", whole, rem, dr);
        gtk_label_set_text(GTK_LABEL(ctx->l_res_mixed), buf);
    } else {
        gtk_label_set_text(GTK_LABEL(ctx->l_res_mixed), "-");
    }
    
    snprintf(buf, sizeof buf, "%.6f", (double)nr / (double)dr);
    gtk_label_set_text(GTK_LABEL(ctx->l_res_dec), buf);
}

GtkWidget *build_frac_calc(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    FracCtx *ctx = g_new0(FracCtx, 1);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);
    gtk_widget_set_margin_top(box, 20); gtk_widget_set_margin_bottom(box, 20);
    
    GtkWidget *hb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_set_halign(hb, GTK_ALIGN_CENTER);
    
    // Frac 1
    GtkWidget *vb1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    ctx->e_num1 = gtk_entry_new(); gtk_widget_set_size_request(ctx->e_num1, 60, -1);
    ctx->e_den1 = gtk_entry_new(); gtk_widget_set_size_request(ctx->e_den1, 60, -1);
    GtkWidget *sep1 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(vb1), ctx->e_num1); gtk_box_append(GTK_BOX(vb1), sep1); gtk_box_append(GTK_BOX(vb1), ctx->e_den1);
    gtk_box_append(GTK_BOX(hb), vb1);
    
    // Op
    const char *ops[] = { "+", "-", "×", "÷", NULL };
    ctx->cb_op = gtk_drop_down_new_from_strings(ops);
    gtk_widget_set_valign(ctx->cb_op, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(hb), ctx->cb_op);
    
    // Frac 2
    GtkWidget *vb2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    ctx->e_num2 = gtk_entry_new(); gtk_widget_set_size_request(ctx->e_num2, 60, -1);
    ctx->e_den2 = gtk_entry_new(); gtk_widget_set_size_request(ctx->e_den2, 60, -1);
    GtkWidget *sep2 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(vb2), ctx->e_num2); gtk_box_append(GTK_BOX(vb2), sep2); gtk_box_append(GTK_BOX(vb2), ctx->e_den2);
    gtk_box_append(GTK_BOX(hb), vb2);
    
    GtkWidget *btn = gtk_button_new_with_label("=");
    gtk_widget_add_css_class(btn, "suggested-action");
    gtk_widget_set_valign(btn, GTK_ALIGN_CENTER);
    g_signal_connect(btn, "clicked", G_CALLBACK(on_frac_calc), ctx);
    gtk_box_append(GTK_BOX(hb), btn);
    
    gtk_box_append(GTK_BOX(box), hb);
    
    GtkWidget *g = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(g), 8); gtk_grid_set_column_spacing(GTK_GRID(g), 16);
    gtk_widget_set_halign(g, GTK_ALIGN_CENTER);
    
    ctx->l_res_frac = gtk_label_new("-");
    ctx->l_res_mixed = gtk_label_new("-");
    ctx->l_res_dec = gtk_label_new("-");
    gtk_widget_add_css_class(ctx->l_res_frac, "helvetia-title");
    gtk_widget_add_css_class(ctx->l_res_mixed, "helvetia-title");
    gtk_widget_add_css_class(ctx->l_res_dec, "helvetia-title");
    
    gtk_grid_attach(GTK_GRID(g), gtk_label_new("Fraction:"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(g), ctx->l_res_frac, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(g), gtk_label_new("Mixed Number:"), 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(g), ctx->l_res_mixed, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(g), gtk_label_new("Decimal:"), 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(g), ctx->l_res_dec, 1, 2, 1, 1);
    
    gtk_box_append(GTK_BOX(box), g);
    return box;
}

// ---------------------------------------------------------
// Triangle Calculator
// ---------------------------------------------------------
typedef struct {
    GtkWidget *e_a; GtkWidget *e_b; GtkWidget *e_c;
    GtkWidget *l_angA; GtkWidget *l_angB; GtkWidget *l_angC;
    GtkWidget *l_area; GtkWidget *l_perim;
} TriCtx;

static void on_tri_calc(GtkButton *btn, gpointer data) {
    (void)btn;
    TriCtx *ctx = data;
    double a = atof(gtk_editable_get_text(GTK_EDITABLE(ctx->e_a)));
    double b = atof(gtk_editable_get_text(GTK_EDITABLE(ctx->e_b)));
    double c = atof(gtk_editable_get_text(GTK_EDITABLE(ctx->e_c)));
    
    if (a <= 0 || b <= 0 || c <= 0 || (a+b <= c) || (a+c <= b) || (b+c <= a)) {
        gtk_label_set_text(GTK_LABEL(ctx->l_angA), "Invalid");
        gtk_label_set_text(GTK_LABEL(ctx->l_angB), "Invalid");
        gtk_label_set_text(GTK_LABEL(ctx->l_angC), "Invalid");
        gtk_label_set_text(GTK_LABEL(ctx->l_area), "Invalid");
        gtk_label_set_text(GTK_LABEL(ctx->l_perim), "Invalid");
        return;
    }
    
    double A = acos((b*b + c*c - a*a)/(2*b*c)) * 180.0 / G_PI;
    double B = acos((a*a + c*c - b*b)/(2*a*c)) * 180.0 / G_PI;
    double C = 180.0 - A - B;
    
    double s = (a+b+c)/2.0;
    double area = sqrt(s*(s-a)*(s-b)*(s-c));
    
    char buf[64];
    snprintf(buf, sizeof buf, "%.2f°", A); gtk_label_set_text(GTK_LABEL(ctx->l_angA), buf);
    snprintf(buf, sizeof buf, "%.2f°", B); gtk_label_set_text(GTK_LABEL(ctx->l_angB), buf);
    snprintf(buf, sizeof buf, "%.2f°", C); gtk_label_set_text(GTK_LABEL(ctx->l_angC), buf);
    snprintf(buf, sizeof buf, "%.4f", area); gtk_label_set_text(GTK_LABEL(ctx->l_area), buf);
    snprintf(buf, sizeof buf, "%.4f", a+b+c); gtk_label_set_text(GTK_LABEL(ctx->l_perim), buf);
}

GtkWidget *build_tri_calc(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    TriCtx *ctx = g_new0(TriCtx, 1);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);
    gtk_widget_set_margin_top(box, 20); gtk_widget_set_margin_bottom(box, 20);
    
    GtkWidget *g_in = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(g_in), 8); gtk_grid_set_column_spacing(GTK_GRID(g_in), 8);
    gtk_widget_set_halign(g_in, GTK_ALIGN_CENTER);
    
    ctx->e_a = gtk_entry_new(); ctx->e_b = gtk_entry_new(); ctx->e_c = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(g_in), gtk_label_new("Side a:"), 0, 0, 1, 1); gtk_grid_attach(GTK_GRID(g_in), ctx->e_a, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(g_in), gtk_label_new("Side b:"), 0, 1, 1, 1); gtk_grid_attach(GTK_GRID(g_in), ctx->e_b, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(g_in), gtk_label_new("Side c:"), 0, 2, 1, 1); gtk_grid_attach(GTK_GRID(g_in), ctx->e_c, 1, 2, 1, 1);
    
    GtkWidget *btn = gtk_button_new_with_label("Solve Triangle (SSS)");
    gtk_widget_add_css_class(btn, "suggested-action");
    g_signal_connect(btn, "clicked", G_CALLBACK(on_tri_calc), ctx);
    gtk_grid_attach(GTK_GRID(g_in), btn, 0, 3, 2, 1);
    
    gtk_box_append(GTK_BOX(box), g_in);
    
    GtkWidget *g = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(g), 8); gtk_grid_set_column_spacing(GTK_GRID(g), 16);
    gtk_widget_set_halign(g, GTK_ALIGN_CENTER);
    
    ctx->l_angA = gtk_label_new("-"); ctx->l_angB = gtk_label_new("-"); ctx->l_angC = gtk_label_new("-");
    ctx->l_area = gtk_label_new("-"); ctx->l_perim = gtk_label_new("-");
    gtk_widget_add_css_class(ctx->l_angA, "helvetia-title"); gtk_widget_add_css_class(ctx->l_angB, "helvetia-title");
    gtk_widget_add_css_class(ctx->l_angC, "helvetia-title"); gtk_widget_add_css_class(ctx->l_area, "helvetia-title");
    gtk_widget_add_css_class(ctx->l_perim, "helvetia-title");
    
    gtk_grid_attach(GTK_GRID(g), gtk_label_new("Angle A:"), 0, 0, 1, 1); gtk_grid_attach(GTK_GRID(g), ctx->l_angA, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(g), gtk_label_new("Angle B:"), 0, 1, 1, 1); gtk_grid_attach(GTK_GRID(g), ctx->l_angB, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(g), gtk_label_new("Angle C:"), 0, 2, 1, 1); gtk_grid_attach(GTK_GRID(g), ctx->l_angC, 1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(g), gtk_label_new("Area:"), 0, 3, 1, 1);    gtk_grid_attach(GTK_GRID(g), ctx->l_area, 1, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(g), gtk_label_new("Perimeter:"), 0, 4, 1, 1); gtk_grid_attach(GTK_GRID(g), ctx->l_perim, 1, 4, 1, 1);
    
    gtk_box_append(GTK_BOX(box), g);
    return box;
}
