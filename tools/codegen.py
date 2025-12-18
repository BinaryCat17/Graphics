import os
import re
import sys

# Regex patterns
STRUCT_PATTERN = re.compile(r'typedef\s+struct\s+(\w+)\s*\{', re.MULTILINE)
REFLECT_PATTERN = re.compile(r'^\s*(.+?);\s*//\s*REFLECT(.*)', re.MULTILINE)
END_STRUCT_PATTERN = re.compile(r'\}\s*(\w+);', re.MULTILINE)

# Map C types to MetaTypeKind
TYPE_MAP = {
    'int': 'META_TYPE_INT',
    'float': 'META_TYPE_FLOAT',
    'char*': 'META_TYPE_STRING',
    'bool': 'META_TYPE_BOOL',
    'size_t': 'META_TYPE_INT',
    'uint32_t': 'META_TYPE_INT',
    'MathNodeType': 'META_TYPE_INT',
    'UiKind': 'META_TYPE_INT',
    'UiLayoutStrategy': 'META_TYPE_INT',
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
            match = REFLECT_PATTERN.match(line)
            if match:
                code_part = match.group(1).strip()
                comment_part = match.group(2)
                is_observable = 'Observable' in comment_part
                
                # "float x, y, width, height" or "MathNode** nodes"
                # Naive C parsing: Type is everything until the last identifier(s)
                # But with commas it's tricky: "float x, y" -> Type "float", vars "x", "y".
                # "MathNode** nodes" -> Type "MathNode**", var "nodes".
                
                # Split by comma to find variables
                vars_part = code_part.split(',')
                
                # The first part contains the Type and the first var
                # e.g. "float x"
                first_decl = vars_part[0].strip()
                
                # Find the split between type and name in first_decl
                # We assume the last word is the name, possibly preceded by stars
                # "float x" -> type="float", name="x"
                # "MathNode** nodes" -> type="MathNode**", name="nodes"
                # "int* a" -> type="int*", name="a"
                
                # Let's tokenize by space
                tokens = first_decl.split()
                if not tokens: continue
                
                # Reconstruct type. 
                # Case 1: "float x" -> tokens=["float", "x"]
                # Case 2: "unsigned int x" -> tokens=["unsigned", "int", "x"]
                # Case 3: "MathNode* n" -> tokens=["MathNode*", "n"]
                # Case 4: "MathNode *n" -> tokens=["MathNode", "*n"]
                
                # We assume standard formatting "Type Name" or "Type* Name" or "Type *Name"
                # Grab base type from everything except last token
                # But checking if last token is just a name or has stars
                
                last_token = tokens[-1]
                base_type_tokens = tokens[:-1]
                
                base_type = " ".join(base_type_tokens)
                first_name = last_token
                
                # Handle "Type *Name" -> base_type="Type", name="*Name" (pointer belongs to name in C syntax strictly, but we want type)
                # We want type="Type*", name="Name"
                
                # Let's clean up stars from name and append to type
                num_stars = first_name.count('*')
                clean_name = first_name.replace('*', '')
                
                full_type = base_type
                if num_stars > 0:
                    full_type += '*' * num_stars
                
                # Add first var
                vars_list = [(clean_name, full_type)]
                
                # Process remaining vars (they inherit base type)
                # e.g. ", y, *z"
                for v in vars_part[1:]:
                    v = v.strip()
                    v_stars = v.count('*')
                    v_name = v.replace('*', '')
                    v_type = base_type + ('*' * v_stars)
                    vars_list.append((v_name, v_type))
                
                for name, c_type in vars_list:
                    # Map to meta type
                    meta_type = TYPE_MAP.get(c_type, 'META_TYPE_STRUCT')
                    
                    # Heuristic for pointers
                    if '*' in c_type:
                        if c_type == 'char*':
                            meta_type = 'META_TYPE_STRING'
                        else:
                            meta_type = 'META_TYPE_POINTER'

                    current_struct['fields'].append({
                        'name': name,
                        'type': meta_type,
                        'c_type': c_type,
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