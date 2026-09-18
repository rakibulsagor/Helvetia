/* ================================================================
 * Scientific Calculator
 * ================================================================ */
typedef struct { GtkWidget *entry; } CalcCtx;

static void on_calc_btn(GtkButton *btn, gpointer ud) {
    CalcCtx *ctx = ud;
    const char *lbl = gtk_button_get_label(btn);
    GtkEditable *edit = GTK_EDITABLE(ctx->entry);
    
    if (g_strcmp0(lbl, "C") == 0) {
        gtk_editable_set_text(edit, "");
        return;
    }
    
    if (g_strcmp0(lbl, "=") == 0) {
        const char *expr = gtk_editable_get_text(edit);
        if (!expr || !*expr) return;
        
        char awk_prog[1024];
        /* Basic sanitization to prevent breaking awk */
        for (const char *p = expr; *p; p++) {
            if (*p == '\'' || *p == '"' || *p == ';' || *p == '{' || *p == '}') {
                gtk_editable_set_text(edit, "Error: Invalid chars");
                return;
            }
        }
        snprintf(awk_prog, sizeof awk_prog, "awk 'BEGIN { print (%s) }'", expr);
        char *res = hv_run_cmd(awk_prog);
        if (res) {
            g_strstrip(res);
            gtk_editable_set_text(edit, res);
            g_free(res);
        } else {
            gtk_editable_set_text(edit, "Error");
        }
        return;
    }
    
    /* Insert text at cursor */
    int pos = gtk_editable_get_position(edit);
    
    /* for functions, append parens automatically */
    if (g_strcmp0(lbl, "sin") == 0 || g_strcmp0(lbl, "cos") == 0 ||
        g_strcmp0(lbl, "tan") == 0 || g_strcmp0(lbl, "log") == 0 ||
        g_strcmp0(lbl, "exp") == 0 || g_strcmp0(lbl, "sqrt") == 0) 
    {
        char fn[16];
        snprintf(fn, sizeof fn, "%s()", lbl);
        gtk_editable_insert_text(edit, fn, -1, &pos);
        /* move cursor inside parens */
        gtk_editable_set_position(edit, pos - 1);
    } else {
        gtk_editable_insert_text(edit, lbl, -1, &pos);
        gtk_editable_set_position(edit, pos);
    }
}

GtkWidget *build_calc_sci(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "0");
    gtk_widget_add_css_class(entry, "helvetia-result");
    gtk_widget_set_margin_bottom(entry, 10);
    gtk_box_append(GTK_BOX(box), entry);
    
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 6);
    gtk_widget_set_halign(grid, GTK_ALIGN_CENTER);
    
    const char *keys[5][5] = {
        { "sin", "cos", "tan", "sqrt", "C" },
        { "7",   "8",   "9",   "/",    "(" },
        { "4",   "5",   "6",   "*",    ")" },
        { "1",   "2",   "3",   "-",    "^" },
        { "0",   ".",   "=",   "+",    "%" }
    };
    
    CalcCtx *ctx = g_new0(CalcCtx, 1);
    ctx->entry = entry;
    g_signal_connect_swapped(box, "destroy", G_CALLBACK(g_free), ctx);
    
    for (int r = 0; r < 5; r++) {
        for (int c = 0; c < 5; c++) {
            GtkWidget *b = gtk_button_new_with_label(keys[r][c]);
            gtk_widget_set_size_request(b, 50, 40);
            g_signal_connect(b, "clicked", G_CALLBACK(on_calc_btn), ctx);
            
            if (g_strcmp0(keys[r][c], "=") == 0) {
                gtk_widget_add_css_class(b, "suggested-action");
            } else if (g_strcmp0(keys[r][c], "C") == 0) {
                gtk_widget_add_css_class(b, "destructive-action");
            }
            
            gtk_grid_attach(GTK_GRID(grid), b, c, r, 1, 1);
        }
    }
    
    g_signal_connect(entry, "activate", G_CALLBACK(on_calc_btn), ctx);
    /* For "activate", the callback receives the entry as the first arg, not the button,
       which would crash `gtk_button_get_label`. Let's create a dedicated activate handler */
    
    gtk_box_append(GTK_BOX(box), grid);
    return box;
}
