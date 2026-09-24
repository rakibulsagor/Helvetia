import os, re
for root, _, files in os.walk('src/modules'):
    for file in files:
        if file.endswith('_module.c'):
            path = os.path.join(root, file)
            with open(path, 'r') as f: content = f.read()
            matches = re.finditer(r'\{\s*(?:\.id\s*=\s*)?"([^"]+)"\s*,\s*(?:\.name\s*=\s*)?"([^"]+)"', content)
            for m in matches: print(f"{file}\t{m.group(1)}\t{m.group(2)}")
