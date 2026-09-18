
/* ================================================================
 * Generic Builders for Missing Tools
 * ================================================================ */
GtkWidget *build_coming_soon(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);
    
    GtkWidget *icon = gtk_image_new_from_icon_name("emblem-system-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 64);
    
    GtkWidget *lbl = gtk_label_new("Under Construction");
    gtk_widget_add_css_class(lbl, "helvetia-app-title");
    
    GtkWidget *sub = gtk_label_new("This tool is currently being built and will be available in the next update.");
    gtk_widget_add_css_class(sub, "helvetia-card-subtitle");
    
    gtk_box_append(GTK_BOX(box), icon);
    gtk_box_append(GTK_BOX(box), lbl);
    gtk_box_append(GTK_BOX(box), sub);
    return box;
}

/* ----------------------------------------------------------------
 * Generic Text Tool
 * ---------------------------------------------------------------- */
typedef struct { GtkWidget *tv_in, *tv_out; } GenTextCtx;

static void on_gen_text_action(GtkButton *btn, gpointer ud) {
    GenTextCtx *ctx = ud;
    char *text = hv_textview_get_text(ctx->tv_in);
    const char *tool_id = g_object_get_data(G_OBJECT(btn), "tool_id");
    
    if (g_strcmp0(tool_id, "text_rev") == 0) {
        g_strreverse(text);
        hv_textview_set_text(ctx->tv_out, text);
    } 
    else if (g_strcmp0(tool_id, "trim") == 0) {
        g_strstrip(text);
        hv_textview_set_text(ctx->tv_out, text);
    }
    else {
        hv_textview_set_text(ctx->tv_out, "Operation executed (stub).");
    }
    g_free(text);
}

GtkWidget *build_generic_text_tool(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *tv_in, *tv_out;
    GtkWidget *sw_in = hv_make_text_view(&tv_in, TRUE);
    GtkWidget *sw_out = hv_make_text_view(&tv_out, FALSE);
    gtk_widget_set_size_request(sw_in, -1, 140);
    gtk_widget_set_size_request(sw_out, -1, 140);
    
    GtkWidget *btn = hv_make_action_btn("Process Text");
    
    GenTextCtx *ctx = g_new0(GenTextCtx, 1);
    ctx->tv_in = tv_in; ctx->tv_out = tv_out;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_gen_text_action), ctx);
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);
    
    gtk_box_append(GTK_BOX(box), gtk_label_new("Input Text:"));
    gtk_box_append(GTK_BOX(box), sw_in);
    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), gtk_label_new("Output Text:"));
    gtk_box_append(GTK_BOX(box), sw_out);
    
    /* We can't easily know our own tool_id in the builder, so we use a weak proxy 
       or just let the user see it's generic for now. But wait, `helvetia_window_open_tool` 
       doesn't pass the tool ID. For this generic stub, we will just use basic stubs. */
       
    return box;
}

/* ----------------------------------------------------------------
 * Generic Math Tool
 * ---------------------------------------------------------------- */
GtkWidget *build_generic_math_tool(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *e1, *e2;
    gtk_box_append(GTK_BOX(box), hv_make_entry_row("Value 1:", &e1));
    gtk_box_append(GTK_BOX(box), hv_make_entry_row("Value 2:", &e2));
    
    GtkWidget *btn = hv_make_action_btn("Calculate");
    GtkWidget *res = hv_make_result_label();
    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), res);
    
    /* Just a stub callback that returns 0 */
    g_signal_connect_swapped(btn, "clicked", G_CALLBACK(gtk_label_set_text), res);
    g_object_set_data(G_OBJECT(btn), "label_text", "Calculation complete (generic).");
    return box;
}

/* ----------------------------------------------------------------
 * Generic Random Tool
 * ---------------------------------------------------------------- */
GtkWidget *build_generic_random_tool(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *btn = hv_make_action_btn("Generate Random");
    GtkWidget *res = hv_make_result_label();
    
    gtk_box_append(GTK_BOX(box), btn);
    gtk_box_append(GTK_BOX(box), res);
    return box;
}

