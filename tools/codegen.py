import os
import re
import sys

# Regex Patterns
ENUM_PATTERN = re.compile(r'typedef\s+enum\s*(\w+)?\s*\{([^}]*)\}\s*(\w+);', re.MULTILINE | re.DOTALL)
STRUCT_PATTERN = re.compile(r'typedef\s+struct\s*(\w+)?\s*\{([^}]*)\}\s*(\w+);', re.MULTILINE | re.DOTALL)
REFLECT_PATTERN = re.compile(r'^\s*(.+?);\s*//\s*REFLECT(.*)', re.MULTILINE)

BASE_TYPES = {
    'int': 'META_TYPE_INT',
    'float': 'META_TYPE_FLOAT',
    'char*': 'META_TYPE_STRING',
    'bool': 'META_TYPE_BOOL',
    'size_t': 'META_TYPE_INT',
    'uint32_t': 'META_TYPE_INT',
    'uint8_t': 'META_TYPE_INT',
    'int32_t': 'META_TYPE_INT',
}

def parse_enum_body(body_text):
    entries = []
    lines = body_text.split('\n')
    clean_body = ""
    for line in lines:
        if '//' in line:
            line = line.split('//')[0]
        clean_body += line + " "
    
    parts = clean_body.split(',')
    for part in parts:
        part = part.strip()
        if not part: continue
        if '=' in part:
            name = part.split('=')[0].strip()
        else:
            name = part
        if name:
            entries.append(name)
    return entries

def scan_files(src_dir):
    enums = {}
    structs = {}
    headers = []
    
    for root, _, files in os.walk(src_dir):
        # Skip backend directories to avoid platform-specific dependencies
        if 'backend' in root:
            continue

        for file in files:
            if file.endswith('.h'):
                path = os.path.join(root, file)
                with open(path, 'r', encoding='utf-8') as f:
                    content = f.read()
                
                has_content = False

                for match in ENUM_PATTERN.finditer(content):
                    body = match.group(2)
                    name = match.group(3)
                    values = parse_enum_body(body)
                    if values:
                        enums[name] = values
                        has_content = True

                for match in STRUCT_PATTERN.finditer(content):
                    body = match.group(2)
                    name = match.group(3)
                    fields = []
                    for f_match in REFLECT_PATTERN.finditer(body):
                        decl = f_match.group(1).strip()
                        tokens = decl.split()
                        var_name = tokens[-1]
                        type_part = " ".join(tokens[:-1])
                        stars = var_name.count('*')
                        var_name = var_name.replace('*', '')
                        if stars:
                            type_part += '*' * stars
                        if '[' in var_name:
                            var_name = var_name.split('[')[0]
                        
                        fields.append({
                            'name': var_name,
                            'c_type': type_part
                        })
                    
                    if fields:
                        structs[name] = fields
                        has_content = True
                
                if has_content:
                    headers.append(path.replace('\\', '/'))
                    
    return enums, structs, headers

def generate_code(enums, structs, headers, output_path):
    with open(output_path, 'w') as f:
        f.write('#include "foundation/meta/reflection.h"\n')
        f.write('#include <string.h>\n')
        f.write('#include <stddef.h>\n')
        f.write('#include <stdbool.h>\n')
        
        for h in headers:
            if 'src/' in h:
                f.write('#include "' + h.split('src/')[1] + '"\n')
            else:
                f.write('#include "' + h + '"\n')
        f.write('\n')

        # --- Generate Enums ---
        f.write('// --- ENUMS ---\n\n')
        for name, values in enums.items():
            f.write('static const MetaEnumValue values_' + name + '[] = {\n')
            for v in values:
                f.write('    { "' + v + '", (int)' + v + ' },\n')
            f.write('};\n\n')

        # --- Generate Struct Fields ---
        f.write('// --- STRUCTS ---\n\n')
        for s_name, fields in structs.items():
            f.write('static const MetaField fields_' + s_name + '[] = {\n')
            for field in fields:
                c_type = field['c_type']
                f_name = field['name']
                kind = 'META_TYPE_STRUCT'
                type_name = '"' + c_type + '"'
                
                if c_type in BASE_TYPES:
                    kind = BASE_TYPES[c_type]
                    type_name = "NULL"
                elif c_type in enums:
                    kind = 'META_TYPE_ENUM'
                    type_name = '"' + c_type + '"'
                elif c_type == 'char*':
                    kind = 'META_TYPE_STRING'
                    type_name = "NULL"
                elif '*' in c_type:
                    kind = 'META_TYPE_POINTER'
                    base = c_type.replace('*', '').strip()
                    type_name = '"' + base + '"'

                f.write('    { "' + f_name + '", ' + kind + ', offsetof(' + s_name + ', ' + f_name + '), ' + type_name + ' },\n')
            f.write('};\n\n')

        # --- Registry ---
        f.write('// --- REGISTRY ---\n\n')
        f.write('static const MetaEnum enum_registry[] = {\n')
        for name, values in enums.items():
             f.write('    { "' + name + '", values_' + name + ', ' + str(len(values)) + ' },\n')
        f.write('    { NULL, NULL, 0 }\n')
        f.write('};\n\n')
        
        f.write('static const MetaStruct struct_registry[] = {\n')
        for name, fields in structs.items():
            f.write('    { "' + name + '", sizeof(' + name + '), fields_' + name + ', ' + str(len(fields)) + ' },\n')
        f.write('    { NULL, 0, NULL, 0 }\n')
        f.write('};\n\n')

        # --- Implementation ---
        f.write('const MetaStruct* meta_get_struct(const char* name) {\n')
        f.write('    for (const MetaStruct* s = struct_registry; s->name; ++s) {\n')
        f.write('        if (strcmp(s->name, name) == 0) return s;\n')
        f.write('    }\n')
        f.write('    return NULL;\n')
        f.write('}\n\n')

        f.write('const MetaEnum* meta_get_enum(const char* name) {\n')
        f.write('    for (const MetaEnum* e = enum_registry; e->name; ++e) {\n')
        f.write('        if (strcmp(e->name, name) == 0) return e;\n')
        f.write('    }\n')
        f.write('    return NULL;\n')
        f.write('}\n\n')
        
        f.write('bool meta_enum_get_value(const MetaEnum* meta_enum, const char* name_str, int* out_value) {\n')
        f.write('    if (!meta_enum || !name_str) return false;\n')
        f.write('    for (size_t i = 0; i < meta_enum->count; ++i) {\n')
        f.write('        if (strcasecmp(meta_enum->values[i].name, name_str) == 0) {\n')
        f.write('            *out_value = meta_enum->values[i].value;\n')
        f.write('            return true;\n')
        f.write('        }\n')
        f.write('        size_t n_len = strlen(name_str);\n')
        f.write('        size_t v_len = strlen(meta_enum->values[i].name);\n')
        f.write('        if (v_len >= n_len) {\n')
        f.write('             const char* suffix = meta_enum->values[i].name + (v_len - n_len);\n')
        f.write('             if (strcasecmp(suffix, name_str) == 0) {\n')
        f.write("                  if (v_len == n_len || *(suffix - 1) == '_') {\n")
        f.write('                       *out_value = meta_enum->values[i].value;\n')
        f.write('                       return true;\n')
        f.write('                  }\n')
        f.write('             }\n')
        f.write('        }\n')
        f.write('    }\n')
        f.write('    return false;\n')
        f.write('}\n\n')

if __name__ == '__main__':
    src_dir = sys.argv[1]
    output_file = sys.argv[2]
    enums, structs, headers = scan_files(src_dir)
    generate_code(enums, structs, headers, output_file)
