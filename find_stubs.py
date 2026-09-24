import os
import re

def main():
    modules_dir = 'src/modules'
    for root, dirs, files in os.walk(modules_dir):
        for file in files:
            if file.endswith('_module.c'):
                path = os.path.join(root, file)
                with open(path, 'r') as f:
                    content = f.read()
                
                print(f"--- {file} ---")
                
                # Match { .id = "...", ... }
                matches = re.finditer(r'\{\s*(?:\.id\s*=\s*)?"([^"]+)"\s*,\s*(?:\.name\s*=\s*)?"([^"]+)"([^}]+)\}', content)
                for m in matches:
                    _id = m.group(1)
                    name = m.group(2)
                    rest = m.group(3)
                    cv_match = re.search(r'\.create_view\s*=\s*([a-zA-Z0-9_]+)', rest)
                    cv = cv_match.group(1) if cv_match else "NULL"
                    if cv == "NULL":
                        print(f"STUB: {_id} ({name})")
                    else:
                        print(f"IMPL: {_id} ({name}) -> {cv}")

if __name__ == '__main__':
    main()
