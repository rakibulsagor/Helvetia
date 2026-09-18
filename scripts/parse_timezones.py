import json

def main():
    with open('timezones.json', 'r') as f:
        data = json.load(f)
    
    timezones = data.get('timezones', [])
    
    # We want to output C code:
    # const char *TIMEZONES[] = {
    #     "UTC",
    #     "Local Time",
    #     "Pacific/Midway",
    #     ...
    #     NULL
    # };
    
    ids = ["UTC", "Local Time"]
    for tz in timezones:
        # Avoid duplicating UTC
        if tz['id'] not in ids:
            ids.append(tz['id'])
            
    with open('timezones_c_array.txt', 'w') as f:
        f.write("static const char *TIMEZONES[] = {\n")
        for tz_id in ids:
            f.write(f'    "{tz_id}",\n')
        f.write("    NULL\n")
        f.write("};\n")
        
if __name__ == '__main__':
    main()
