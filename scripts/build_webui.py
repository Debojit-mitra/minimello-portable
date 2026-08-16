import os
import gzip

Import("env")

def create_web_ui_header(source_dir, output_file):
    print("Generating Web UI PROGMEM header...")
    files = ["index.html", "style.css", "app.js"]
    
    with open(output_file, 'w') as out:
        out.write('#pragma once\n')
        out.write('#include <Arduino.h>\n\n')
        
        for file in files:
            file_path = os.path.join(source_dir, file)
            if not os.path.exists(file_path):
                print(f"Warning: {file_path} not found!")
                continue
                
            var_name = file.replace('.', '_')
            
            with open(file_path, 'rb') as f:
                data = f.read()
                
            compressed_data = gzip.compress(data, compresslevel=9)
            hex_data = ', '.join([f'0x{b:02x}' for b in compressed_data])
            
            out.write(f'const uint8_t web_{var_name}[] PROGMEM = {{\n')
            
            # Format 16 bytes per line
            hex_array = hex_data.split(', ')
            for i in range(0, len(hex_array), 16):
                out.write('    ' + ', '.join(hex_array[i:i+16]) + ',\n')
                
            out.write('};\n')
            out.write(f'const size_t web_{var_name}_len = {len(compressed_data)};\n\n')
            
    print(f"Generated {output_file} successfully.")

create_web_ui_header('data', 'src/web_ui_data.h')
