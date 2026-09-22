import os
import glob
import re

def process_file(filepath):
    with open(filepath, 'r') as f:
        content = f.read()
    
    original_content = content

    # Fix the root to st->root in zoom callbacks
    content = content.replace('G_CALLBACK(image_zoom_out), root);', 'G_CALLBACK(image_zoom_out), st->root);')
    content = content.replace('G_CALLBACK(image_zoom_in), root);', 'G_CALLBACK(image_zoom_in), st->root);')
    content = content.replace('G_CALLBACK(image_zoom_reset), root);', 'G_CALLBACK(image_zoom_reset), st->root);')

    # Fix gdk_texture_new_for_pixbuf deprecations:
    # 1. gtk_picture_set_paintable(GTK_PICTURE(X), GDK_PAINTABLE(gdk_texture_new_for_pixbuf(Y))); -> gtk_picture_set_pixbuf(GTK_PICTURE(X), Y);
    content = re.sub(
        r'gtk_picture_set_paintable\(\s*GTK_PICTURE\((.*?)\),\s*GDK_PAINTABLE\(gdk_texture_new_for_pixbuf\((.*?)\)\)\s*\);',
        r'gtk_picture_set_pixbuf(GTK_PICTURE(\1), \2);',
        content
    )

    # 2. GdkTexture *t = gdk_texture_new_for_pixbuf(src);
    #    gtk_picture_set_paintable(GTK_PICTURE(st->picture), GDK_PAINTABLE(t));
    #    g_object_unref(t);
    # -> gtk_picture_set_pixbuf(GTK_PICTURE(st->picture), src);
    content = re.sub(
        r'GdkTexture\s*\*([a-zA-Z0-9_]+)\s*=\s*gdk_texture_new_for_pixbuf\((.*?)\);\s*gtk_picture_set_paintable\(\s*GTK_PICTURE\((.*?)\),\s*GDK_PAINTABLE\(\1\)\s*\);\s*g_object_unref\(\1\);',
        r'gtk_picture_set_pixbuf(GTK_PICTURE(\3), \2);',
        content
    )
    
    # 3. Same as above without unref
    content = re.sub(
        r'GdkTexture\s*\*([a-zA-Z0-9_]+)\s*=\s*gdk_texture_new_for_pixbuf\((.*?)\);\s*gtk_picture_set_paintable\(\s*GTK_PICTURE\((.*?)\),\s*GDK_PAINTABLE\(\1\)\s*\);',
        r'gtk_picture_set_pixbuf(GTK_PICTURE(\3), \2);',
        content
    )

    # 4. In image_thumbnail_grid.c, warning: ‘gdk_cairo_set_source_pixbuf’ is deprecated. Let's fix that.
    # gdk_cairo_set_source_pixbuf(cr, pb, x, y);
    # The non-deprecated way in GTK4 is not directly available because gdk_cairo_set_source_pixbuf IS deprecated.
    # We should just ignore it for now or replace with cairo surface creation.
    # Wait, actually gdk_cairo_set_source_pixbuf is the only way unless we use GdkTexture. Let's just suppress it or leave it, it's just a warning.

    if original_content != content:
        with open(filepath, 'w') as f:
            f.write(content)
        print(f"Fixed {filepath}")

if __name__ == '__main__':
    tools_dir = 'src/modules/image/tools'
    for f in glob.glob(os.path.join(tools_dir, '*.c')):
        process_file(f)
