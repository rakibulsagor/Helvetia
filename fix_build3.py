import os
import glob

def process_file(filepath):
    with open(filepath, 'r') as f:
        content = f.read()

    original_content = content

    if 'build_editor_shell' in content or 'build_curves_shell' in content or 'build_levels_shell' in content:
        # These functions don't take a root parameter, they use st->root.
        content = content.replace('G_CALLBACK(image_zoom_out), root);', 'G_CALLBACK(image_zoom_out), st->root);')
        content = content.replace('G_CALLBACK(image_zoom_in), root);', 'G_CALLBACK(image_zoom_in), st->root);')
        content = content.replace('G_CALLBACK(image_zoom_reset), root);', 'G_CALLBACK(image_zoom_reset), st->root);')

    if original_content != content:
        with open(filepath, 'w') as f:
            f.write(content)
        print(f"Fixed {filepath}")

if __name__ == '__main__':
    tools_dir = 'src/modules/image/tools'
    for f in glob.glob(os.path.join(tools_dir, '*.c')):
        process_file(f)
