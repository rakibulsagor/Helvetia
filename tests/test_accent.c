#include <adwaita.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>

static void test_accent_colors(void) {
    const char *env_accent = g_getenv("ADW_ACCENT_COLOR");
    if (!env_accent) {
        env_accent = "default";
    }
    g_print("Testing accent: %s\n", env_accent);

    // Load our theme.css to verify the aliases
    GtkCssProvider *provider = gtk_css_provider_new();
    // In the test environment, we are run from the build dir, the source is in ../src/ui/theme.css
    gtk_css_provider_load_from_path(provider, "../src/ui/theme.css");
    gtk_style_context_add_provider_for_display(gdk_display_get_default(),
                                               GTK_STYLE_PROVIDER(provider),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    // 1. Get the raw named color from Adwaita
    GtkWidget *dummy = gtk_label_new("");
    GtkStyleContext *dummy_ctx = gtk_widget_get_style_context(dummy);
    GdkRGBA expected_bg;
    gboolean found = gtk_style_context_lookup_color(dummy_ctx, "accent_bg_color", &expected_bg);
    g_assert_true(found);

    // 2. Create a widget with a custom class that uses var(--accent-bg-color)
    // .task-strip progressbar progress uses var(--accent-bg-color)
    // Or we can just use our own inline test snippet if theme.css is too complex to mock widget structure.
    
    // Instead of mocking the exact widget structure, let's inject a simple test rule:
    GtkCssProvider *test_provider = gtk_css_provider_new();
    const char *test_css = ".test-accent-widget { color: var(--accent-bg-color); }";
    gtk_css_provider_load_from_string(test_provider, test_css);
    gtk_style_context_add_provider_for_display(gdk_display_get_default(),
                                               GTK_STYLE_PROVIDER(test_provider),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);

    GtkWidget *test_widget = gtk_label_new("Test");
    gtk_widget_add_css_class(test_widget, "test-accent-widget");
    GtkStyleContext *test_ctx = gtk_widget_get_style_context(test_widget);

    GdkRGBA actual_color;
    found = gtk_style_context_lookup_color(test_ctx, "theme_fg_color", &actual_color); 
    // Wait, lookup_color for 'color' property? No, lookup_color looks up named colors. 
    // To get the computed CSS property 'color', we use gtk_style_context_get_color.
    gtk_style_context_get_color(test_ctx, &actual_color);

    g_print("Expected (accent_bg_color): rgba(%.3f, %.3f, %.3f, %.3f)\n",
            expected_bg.red, expected_bg.green, expected_bg.blue, expected_bg.alpha);
    g_print("Actual (var(--accent-bg-color)): rgba(%.3f, %.3f, %.3f, %.3f)\n",
            actual_color.red, actual_color.green, actual_color.blue, actual_color.alpha);

    // Compare them
    // Allow small floating point differences
    g_assert_cmpfloat(ABS(expected_bg.red - actual_color.red), <, 0.01);
    g_assert_cmpfloat(ABS(expected_bg.green - actual_color.green), <, 0.01);
    g_assert_cmpfloat(ABS(expected_bg.blue - actual_color.blue), <, 0.01);

    g_object_unref(provider);
    g_object_unref(test_provider);
    g_object_ref_sink(dummy);
    g_object_unref(dummy);
    g_object_ref_sink(test_widget);
    g_object_unref(test_widget);
}

int main(int argc, char *argv[]) {
    gtk_init();
    adw_init();

    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/theme/accent_colors", test_accent_colors);
    return g_test_run();
}
