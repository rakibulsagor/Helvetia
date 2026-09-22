import os
import glob
import re

def process_file(filepath):
    with open(filepath, 'r') as f:
        content = f.read()

    original_content = content
    modified = False

    # 1. Add double zoom; to State struct
    if 'double zoom;' not in content:
        # Find struct definition
        content = re.sub(
            r'(GtkWidget \*stack, \*picture,)',
            r'double     zoom;\n    \1',
            content, count=1
        )
        if 'double     zoom;' not in content:
            # Maybe it doesn't have stack, picture exactly like that
            content = re.sub(
                r'(GtkWidget \*picture;)',
                r'double zoom;\n    \1',
                content, count=1
            )
        modified = True

    # 2. Modify build_shell picture/scroller
    # We look for GtkWidget *pic = gtk_picture_new(); ... st->overlay = overlay;
    pic_pattern = re.compile(
        r'GtkWidget \*pic = gtk_picture_new\(\);\s*'
        r'gtk_picture_set_content_fit\(GTK_PICTURE\(pic\), GTK_CONTENT_FIT_CONTAIN\);\s*'
        r'st->picture = pic;\s*'
        r'GtkWidget \*overlay = gtk_overlay_new\(\);\s*'
        r'gtk_overlay_set_child\(GTK_OVERLAY\(overlay\), pic\);\s*'
        r'st->overlay = overlay;', re.MULTILINE
    )
    if pic_pattern.search(content):
        replacement = '''GtkWidget *pic = gtk_picture_new();
    gtk_picture_set_can_shrink(GTK_PICTURE(pic), FALSE);
    gtk_picture_set_content_fit(GTK_PICTURE(pic), GTK_CONTENT_FIT_CONTAIN);
    gtk_widget_add_css_class(pic, "image-viewer-canvas");
    st->picture = pic;

    GtkWidget *scroller = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), pic);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    GtkWidget *overlay = gtk_overlay_new();
    gtk_overlay_set_child(GTK_OVERLAY(overlay), scroller);
    gtk_widget_set_vexpand(overlay, TRUE);
    st->overlay = overlay;'''
        content = pic_pattern.sub(replacement, content)
        modified = True
    else:
        # Try another variation for image_draw.c etc.
        pic_pattern2 = re.compile(
            r'GtkWidget \*pic = gtk_picture_new\(\);\s*'
            r'st->picture = pic;\s*'
            r'GtkWidget \*overlay = gtk_overlay_new\(\);\s*'
            r'gtk_overlay_set_child\(GTK_OVERLAY\(overlay\), pic\);\s*'
            r'st->overlay = overlay;', re.MULTILINE
        )
        if pic_pattern2.search(content):
            replacement = '''GtkWidget *pic = gtk_picture_new();
    gtk_picture_set_can_shrink(GTK_PICTURE(pic), FALSE);
    gtk_picture_set_content_fit(GTK_PICTURE(pic), GTK_CONTENT_FIT_CONTAIN);
    gtk_widget_add_css_class(pic, "image-viewer-canvas");
    st->picture = pic;

    GtkWidget *scroller = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), pic);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    GtkWidget *overlay = gtk_overlay_new();
    gtk_overlay_set_child(GTK_OVERLAY(overlay), scroller);
    gtk_widget_set_vexpand(overlay, TRUE);
    st->overlay = overlay;'''
            content = pic_pattern2.sub(replacement, content)
            modified = True

    # 3. Add zoom controls to toolbar
    zoom_controls = '''
    gtk_box_append(GTK_BOX(bar), gtk_separator_new(GTK_ORIENTATION_VERTICAL));
    GtkWidget *z_out = gtk_button_new_from_icon_name("zoom-out-symbolic");
    GtkWidget *z_in = gtk_button_new_from_icon_name("zoom-in-symbolic");
    GtkWidget *z_1 = gtk_button_new_from_icon_name("zoom-original-symbolic");
    gtk_widget_add_css_class(z_out, "flat");
    gtk_widget_add_css_class(z_in, "flat");
    gtk_widget_add_css_class(z_1, "flat");
    g_signal_connect_swapped(z_out, "clicked", G_CALLBACK(image_zoom_out), root);
    g_signal_connect_swapped(z_in, "clicked", G_CALLBACK(image_zoom_in), root);
    g_signal_connect_swapped(z_1, "clicked", G_CALLBACK(image_zoom_reset), root);
    gtk_box_append(GTK_BOX(bar), z_out);
    gtk_box_append(GTK_BOX(bar), z_1);
    gtk_box_append(GTK_BOX(bar), z_in);
'''
    if 'zoom-out-symbolic' not in content:
        # Find where to insert it. After reset btn if it exists, or just before returning overlay/stack
        # Let's find: `gtk_box_append(GTK_BOX(bar), reset);`
        if 'gtk_box_append(GTK_BOX(bar), reset);' in content:
            content = content.replace('gtk_box_append(GTK_BOX(bar), reset);', 'gtk_box_append(GTK_BOX(bar), reset);\n' + zoom_controls)
        elif 'gtk_box_append(GTK_BOX(editor), bar);' in content:
            content = content.replace('gtk_box_append(GTK_BOX(editor), bar);', zoom_controls + '\n    gtk_box_append(GTK_BOX(editor), bar);')
        modified = True

    # 4. Wire zoom in every tool's create function
    if 'image_register_zoom(root, st->picture, &st->zoom);' not in content:
        # Find `return root;` at the end of create functions
        # There might be multiple create functions.
        # We find `g_object_set_data_full(G_OBJECT(root), "..., st, ...);` and insert after it.
        # It's safer to just do a regex sub
        content = re.sub(
            r'(g_object_set_data_full\(G_OBJECT\(root\), .*?st,\s*\(GDestroyNotify\).*?;\n)',
            r'\1    image_register_zoom(root, st->picture, &st->zoom);\n    image_install_zoom_shortcuts(root);\n',
            content, flags=re.DOTALL
        )
        modified = True

    # 5. Set st->zoom = 1.0; on init and on drop
    if 'st->zoom = 1.0;' not in content:
        # Find where undo_stack is initialized or similar
        content = re.sub(r'(st->undo_stack = g_ptr_array_new\(\);)', r'\1\n    st->zoom = 1.0;', content)
        # For on_drop_common, the user didn't specify exactly, but said "set st->zoom = 1.0; on drop"
        # However, we can just look for `first_original = gdk_pixbuf_copy(st->current);` or similar drop code.
        content = re.sub(r'(st->first_original = gdk_pixbuf_copy\(st->current\);)', r'\1\n    st->zoom = 1.0;\n    image_zoom_apply_if_exists(st);', content) # wait, we can just call zoom_reset? No, just setting zoom is fine. Actually, image_zoom_reset(st->root) is better if root is available. I'll just set st->zoom = 1.0. 
        # Actually, let's just do st->zoom = 1.0 in on_drop_common if it exists in the file (like image_draw, image_stamp). wait, on_drop_common is in the file? Yes, usually defined in each file.
        content = re.sub(r'(gtk_widget_queue_draw\(st->draw_area\);)', r'\1\n    st->zoom = 1.0;', content) # a bit hacky, let's just look for on_drop_common signature
        modified = True

    # Specifically for on_drop_common in the files:
    content = re.sub(
        r'(static void on_drop_common.*?)\{',
        r'\1{\n    st->zoom = 1.0;',
        content, flags=re.DOTALL
    )

    if original_content != content:
        with open(filepath, 'w') as f:
            f.write(content)
        print(f"Patched {filepath}")

if __name__ == '__main__':
    tools_dir = 'src/modules/image/tools'
    for f in glob.glob(os.path.join(tools_dir, '*.c')):
        process_file(f)
