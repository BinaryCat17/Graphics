import os
import sys
import re

def parse_real_yaml(path):
    data = {'service': {}, 'components': [], 'includes': []}
    
    current_section = None
    current_component = None
    
    with open(path, 'r') as f:
        content = f.read()

    for line in content.splitlines():
        line = line.strip()
        if not line or line.startswith('#'): continue
        
        if line.startswith('service:'):
            current_section = 'service'
            continue
        elif line.startswith('components:'):
            current_section = 'components'
            continue
        elif line.startswith('includes:'):
            current_section = 'includes'
            continue
            
        if current_section == 'service':
            if ':' in line:
                k, v = line.split(':', 1)
                k, v = k.strip(), v.strip()
                if v.startswith('"'): v = v[1:-1]
                data['service'][k] = v
                
        elif current_section == 'includes':
            if line.startswith('- '):
                inc = line[2:].strip()
                if inc.startswith('"'): inc = inc[1:-1]
                data['includes'].append(inc)

        elif current_section == 'components':
            if line.startswith('- name:'):
                name = line.split(':', 1)[1].strip().strip('"')
                current_component = {'name': name, 'fields': []}
                data['components'].append(current_component)
            elif line.startswith('- {') and current_component:
                inner = line[line.find('{')+1 : line.find('}')]
                field = {}
                for part in inner.split(','):
                    if ':' in part:
                        pk, pv = part.split(':', 1)
                        field[pk.strip()] = pv.strip().strip('"')
                current_component['fields'].append(field)
                
    return data

def get_short_name(comp_name):
    if comp_name.endswith("Component"):
        return comp_name[:-9].lower()
    return comp_name.lower()

def generate_code(services, output_dir):
    h_path = os.path.join(output_dir, "services_registry.h")
    c_path = os.path.join(output_dir, "services_registry.c")
    
    with open(h_path, 'w') as f:
        f.write("#ifndef SERVICES_REGISTRY_H\n")
        f.write("#define SERVICES_REGISTRY_H\n\n")
        f.write("#include \"core/state/state_manager.h\"\n")
        f.write("#include \"services/manager/service_manager.h\"\n")
        f.write("#include <stdbool.h>\n\n")

        # Emit Includes
        unique_includes = set()
        for s in services:
            for inc in s.get('includes', []):
                unique_includes.add(inc)
        
        for inc in sorted(unique_includes):
            f.write(f"#include \"{inc}\"\n")
        f.write("\n")
        
        # Define component ID constants and externs
        for s in services:
            for c in s['components']:
                short_name = get_short_name(c['name'])
                define_name = f"STATE_COMPONENT_{short_name.upper()}"
                
                f.write(f"#define {define_name} \"{short_name}\"\n")
                f.write(f"extern int TYPE_ID_{short_name.upper()};\n")

        f.write("\n")

        # Declare structs
        for s in services:
            for c in s['components']:
                f.write(f"typedef struct {c['name']} {{\n")
                for field in c['fields']:
                     f.write(f"    {field['type']} {field['name']};\n")
                f.write(f"}} {c['name']};\n\n")
                
        f.write("void Generated_RegisterComponents(StateManager* sm);\n")
        f.write("bool Generated_StartServices(ServiceManager* sm, void* services, const ServiceConfig* config);\n")
        f.write("\n#endif // SERVICES_REGISTRY_H\n")

    with open(c_path, 'w') as f:
        f.write("#include \"services_registry.h\"\n")
        f.write("#include <stdio.h>\n")
        
        for s in services:
             if 'header' in s['service']:
                 f.write(f"#include \"{s['service']['header']}\"\n")
        
        f.write("\n")
        
        # Define Global ID variables
        for s in services:
            for c in s['components']:
                short_name = get_short_name(c['name'])
                f.write(f"int TYPE_ID_{short_name.upper()} = -1;\n")
        
        f.write("\nvoid Generated_RegisterComponents(StateManager* sm) {\n")
        for s in services:
            for c in s['components']:
                short_name = get_short_name(c['name'])
                define_name = f"STATE_COMPONENT_{short_name.upper()}"
                var_name = f"TYPE_ID_{short_name.upper()}"
                
                f.write(f"    state_manager_register_type(sm, {define_name}, sizeof({c['name']}), 1, &{var_name});\n") 
        f.write("}\n\n")
        
        f.write("bool Generated_StartServices(ServiceManager* sm, void* services, const ServiceConfig* config) {\n")
        for s in services:
            runtime = s['service'].get('runtime', 'true').lower()
            if runtime == 'false':
                continue

            snake_name = re.sub(r'(?<!^)(?=[A-Z])', '_', s['service']['name']).lower()
            descriptor_func = f"{snake_name}_descriptor"
            f.write(f"    if (!service_manager_register(sm, {descriptor_func}())) return false;\n")
            
        f.write("\n    return service_manager_start(sm, services, config);\n")
        f.write("}\n")

def main():
    root_dir = "src/services"
    output_dir = "generated"
    services = []
    
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    for root, dirs, files in os.walk(root_dir):
        for file in files:
            if file == "service.yaml":
                path = os.path.join(root, file)
                print(f"Processing {path}...")
                data = parse_real_yaml(path)
                services.append(data)

    generate_code(services, output_dir)
    print("Done.")

if __name__ == "__main__":
    main()