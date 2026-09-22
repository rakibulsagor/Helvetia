import os
import glob
import re

def process_file(filepath):
    with open(filepath, 'r') as f:
        content = f.read()

    original_content = content

    # Revert gtk_picture_set_pixbuf back to gdk_texture_new_for_pixbuf
    # We replaced: gtk_picture_set_pixbuf(GTK_PICTURE(X), Y) -> gtk_picture_set_paintable(GTK_PICTURE(X), GDK_PAINTABLE(gdk_texture_new_for_pixbuf(Y)))
    content = re.sub(
        r'gtk_picture_set_pixbuf\(\s*GTK_PICTURE\((.*?)\),\s*(.*?)\s*\);',
        r'gtk_picture_set_paintable(GTK_PICTURE(\1), GDK_PAINTABLE(gdk_texture_new_for_pixbuf(\2)));',
        content
    )

    # Remove g_object_unref(t); where we had removed t
    content = re.sub(r'g_object_unref\(\s*t\s*\);', r'', content)

    # Fix root vs st->root in autocrop and others that might not have root in the struct
    # For AutocropState, it doesn't have `root`, we should use `st->picture`'s top-level or pass root.
    # Wait, if Autocrop doesn't have `root`, how does it get `root`?
    # In autocrop: build_shell(st, ..., root); It takes `root` as parameter!
    # So using `root` in g_signal_connect inside build_shell is perfectly fine for autocrop!
    # Wait, fix_build.py replaced `root);` with `st->root);` in ALL files!
    # So I just need to fix that. If `root` is a parameter of `build_shell`, we should just use `root` instead of `st->root`.
    # Let's revert st->root back to root in build_shell parameters. But how do we know if it's `build_shell`?
    # Actually, in `image_color.c`, `build_editor_shell` doesn't have `root` parameter, but `st` does have `root`.
    # Let's check `image_autocrop.c`: `AutocropState` doesn't have `root` field. But `build_shell` does have `root` param!
    # I'll just change `st->root` back to `root` where `AutocropState` is used, or maybe universally revert and manually fix `image_color.c` etc.
    # It's easier: change `st->root` back to `root` everywhere, then for `image_color.c` and similar, change `root` to `st->root` explicitly.
    content = content.replace('G_CALLBACK(image_zoom_out), st->root);', 'G_CALLBACK(image_zoom_out), root);')
    content = content.replace('G_CALLBACK(image_zoom_in), st->root);', 'G_CALLBACK(image_zoom_in), root);')
    content = content.replace('G_CALLBACK(image_zoom_reset), st->root);', 'G_CALLBACK(image_zoom_reset), root);')

    if original_content != content:
        with open(filepath, 'w') as f:
            f.write(content)
        print(f"Fixed {filepath}")

if __name__ == '__main__':
    tools_dir = 'src/modules/image/tools'
    for f in glob.glob(os.path.join(tools_dir, '*.c')):
        process_file(f)
