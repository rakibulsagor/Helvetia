import re

with open('/home/d4rkman/Downloads/Files/1.Study/projects/Swiss/src/modules/utility/timezone_studio.c', 'r') as f:
    content = f.read()
    
with open('/home/d4rkman/Downloads/Files/1.Study/projects/Swiss/scripts/timezones_c_array.txt', 'r') as f:
    new_array = f.read()

# Replace the TIMEZONES array
pattern = r'static const char \*TIMEZONES\[\] = \{.*?\n\};\n'
new_content = re.sub(pattern, new_array, content, flags=re.DOTALL)

with open('/home/d4rkman/Downloads/Files/1.Study/projects/Swiss/src/modules/utility/timezone_studio.c', 'w') as f:
    f.write(new_content)
