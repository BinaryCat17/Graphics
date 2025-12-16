import os

# Map of old prefix -> new prefix
# Only applied if the line starts with #include
replacements = [
    ('coordinate_systems/', 'core/math/'),
    ('memory/', 'core/memory/'),
    ('platform/', 'core/platform/'),
    ('config/', 'core/config/'),
    ('state/', 'core/state/'),
    
    # Specific file replacements to avoid matching data paths
    ('assets/assets.h', 'services/assets/assets.h'),
    
    ('scene/', 'services/scene/'),
    ('ui/', 'services/ui/'),
    
    ('render_runtime/', 'services/render/runtime/'),
    ('render_service/', 'services/render/service/'),
    
    # render/ was src/render. Now src/services/render/backend/common/ (mostly)
    # But wait, src/render contained 'common' and 'vulkan'.
    # So 'render/common/' -> 'services/render/backend/common/'
    # 'render/vulkan/' -> 'services/render/backend/vulkan/'
    ('render/common/', 'services/render/backend/common/'),
    ('render/vulkan/', 'services/render/backend/vulkan/'),
    
    ('services/service_manager.h', 'services/manager/service_manager.h'),
    ('services/service.h', 'services/manager/service.h'),
    ('services/service_events.h', 'services/manager/service_events.h'),
    
    # App headers were in src/app. Now in src/app.
    # Includes like "app/app_services.h" work if src is include path.
    # Previously 'src/app/app_services.h' was included as "app/app_services.h".
    # This remains valid.
]

def process_file(path):
    with open(path, 'r') as f:
        lines = f.readlines()
    
    modified = False
    new_lines = []
    for line in lines:
        l = line.strip()
        if l.startswith('#include'):
            # Check for quotes
            start_q = line.find('"')
            end_q = line.rfind('"')
            if start_q != -1 and end_q != -1:
                inc = line[start_q+1:end_q]
                
                # Apply replacements
                new_inc = inc
                for old, new in replacements:
                    if inc.startswith(old):
                        new_inc = new + inc[len(old):]
                        break # One replacement per line
                
                if new_inc != inc:
                    line = line.replace(f'"{inc}"', f'"{new_inc}"')
                    modified = True
        new_lines.append(line)
        
    if modified:
        print(f"Updating {path}")
        with open(path, 'w') as f:
            f.writelines(new_lines)

for root, dirs, files in os.walk("src"):
    for file in files:
        if file.endswith('.c') or file.endswith('.h'):
            process_file(os.path.join(root, file))
            
for root, dirs, files in os.walk("tests"):
    for file in files:
        if file.endswith('.c') or file.endswith('.h'):
            process_file(os.path.join(root, file))
