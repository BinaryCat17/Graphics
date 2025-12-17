import os
import re
import sys

# Regex patterns
STRUCT_PATTERN = re.compile(r'typedef\s+struct\s+(\w+)\s*\{', re.MULTILINE)
FIELD_PATTERN = re.compile(r'\s*(\w+\*?)\s+(\w+);\s*//\s*REFLECT', re.MULTILINE)
END_STRUCT_PATTERN = re.compile(r'\}\s*(\w+);', re.MULTILINE)

# Map C types to MetaTypeKind
TYPE_MAP = {
    'int': 'META_TYPE_INT',
    'float': 'META_TYPE_FLOAT',
    'char*': 'META_TYPE_STRING',
    'bool': 'META_TYPE_BOOL',
    'size_t': 'META_TYPE_INT',
    'MathNodeType': 'META_TYPE_INT', # Enums as ints for now
}

def parse_header(file_path):
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    structs = []
    
    # Simple state machine parser
    lines = content.split('\n')
    current_struct = None
    
    for line in lines:
        line = line.strip()
        
        # Start of struct
        if line.startswith('typedef struct'):
            match = re.search(r'typedef struct\s+(\w+)', line)
            if not match: # Try brace on same line
                match = re.search(r'typedef struct\s+(\w+)\s*\{', line)
            
            if match:
                current_struct = {
                    'name': match.group(1),
                    'fields': []
                }
                continue

        if current_struct:
            # Check for end of struct
            if line.startswith('}') or (line.startswith('} ') and ';' in line):
                structs.append(current_struct)
                current_struct = None
                continue

            # Check for reflected field
            if '// REFLECT' in line:
                comment_part = line.split('//')[1]
                is_observable = 'Observable' in comment_part
                
                # Remove comment
                code_part = line.split('//')[0].strip()
                # Parse type and name: "float value;"
                # Remove semicolon
                if code_part.endswith(';'):
                    code_part = code_part[:-1]
                
                parts = code_part.split()
                if len(parts) >= 2:
                    name = parts[-1]
                    type_str = " ".join(parts[:-1])
                    
                    # Handle pointers sticky to type or name
                    if name.startswith('*'):
                        type_str += '*'
                        name = name[1:]
                    
                    meta_type = TYPE_MAP.get(type_str, 'META_TYPE_STRUCT')
                    
                    # Heuristic for pointers to structs (handle **, *** etc)
                    if type_str.endswith('*') and meta_type == 'META_TYPE_STRUCT' and type_str != 'char*':
                        meta_type = 'META_TYPE_POINTER'
                        while type_str.endswith('*'):
                            type_str = type_str[:-1].strip()

                    current_struct['fields'].append({
                        'name': name,
                        'type': meta_type,
                        'c_type': type_str,
                        'observable': is_observable
                    })

    return structs

def generate_c_code(structs, headers, output_path):
    # 1. Generate Registry (Existing)
    with open(output_path, 'w') as f:
        f.write('#include "foundation/meta/reflection.h"\n')
        f.write('#include <string.h>\n')
        f.write('#include <stddef.h>\n')
        
        # Include scanned headers
        for h in headers:
            # Calculate relative path from src root roughly
            if 'src/' in h:
                rel = h.split('src/')[1]
                f.write(f'#include "{rel}"\n')
            else:
                f.write(f'#include "{h}"\n')
        
        f.write('\n')

        # Generate Field Arrays
        for s in structs:
            if not s['fields']: continue
            f.write(f'static const MetaField fields_{s["name"]}[] = {{\n')
            for field in s['fields']:
                f.write(f'    {{ "{field["name"]}", {field["type"]}, offsetof({s["name"]}, {field["name"]}), "{field["c_type"]}" }},\n')
            f.write('};\n\n')

        # Generate Registry Array
        f.write('static const MetaStruct registry[] = {\n')
        for s in structs:
            field_ptr = f'fields_{s["name"]}' if s['fields'] else 'NULL'
            f.write(f'    {{ "{s["name"]}", sizeof({s["name"]}), {field_ptr}, {len(s["fields"])} }},\n')
        f.write('    { NULL, 0, NULL, 0 }\n')
        f.write('};\n\n')

        # Find Function
        f.write('const MetaStruct* meta_registry_find(const char* name) {\n')
        f.write('    for (const MetaStruct* s = registry; s->name; ++s) {\n')
        f.write('        if (strcmp(s->name, name) == 0) return s;\n')
        f.write('    }\n')
        f.write('    return NULL;\n')
        f.write('}\n')

    # 2. Generate Accessors
    base_dir = os.path.dirname(output_path)
    h_path = os.path.join(base_dir, 'accessors.h')
    c_path = os.path.join(base_dir, 'accessors.c')
    
    with open(h_path, 'w') as fh, open(c_path, 'w') as fc:
        fh.write('#pragma once\n\n')
        fh.write('#include <stdbool.h>\n')
        
        fc.write('#include "accessors.h"\n')
        fc.write('#include "foundation/event/event_system.h"\n')
        fc.write('#include <string.h>\n')
        
        # Include headers in accessor.c too
        for h in headers:
            if 'src/' in h:
                rel = h.split('src/')[1]
                fc.write(f'#include "{rel}"\n')
                fh.write(f'#include "{rel}"\n') # Also need types in header
            else:
                fc.write(f'#include "{h}"\n')
                fh.write(f'#include "{h}"\n')

        fh.write('\n')
        fc.write('\n')
        
        for s in structs:
            for field in s['fields']:
                if field.get('observable'):
                    # Setter Signature
                    func_name = f'{s["name"]}_set_{field["name"]}'
                    # Pointer type for struct?
                    # The struct type name is s['name'].
                    
                    fh.write(f'void {func_name}({s["name"]}* obj, {field["c_type"]} value);\n')
                    
                    fc.write(f'void {func_name}({s["name"]}* obj, {field["c_type"]} value) {{\n')
                    fc.write(f'    if (obj) {{\n')
                    # Simple comparison (won't work for structs/arrays but fine for primitives/pointers)
                    # For strings, we might need strcmp, but let's assume primitives for now as per roadmap example
                    fc.write(f'        obj->{field["name"]} = value;\n') 
                    fc.write(f'        event_emit(obj, "{field["name"]}");\n')
                    fc.write(f'    }}\n')
                    fc.write(f'}}\n\n')

if __name__ == '__main__':
    src_dir = sys.argv[1]
    output_file = sys.argv[2]
    
    all_structs = []
    headers = []

    for root, dirs, files in os.walk(src_dir):
        for file in files:
            if file.endswith('.h'):
                path = os.path.join(root, file)
                structs = parse_header(path)
                if structs:
                    # Filter only structs that have at least one REFLECT field
                    reflected_structs = [s for s in structs if s['fields']]
                    if reflected_structs:
                        all_structs.extend(reflected_structs)
                        headers.append(path.replace('\\', '/'))

    generate_c_code(all_structs, headers, output_file)