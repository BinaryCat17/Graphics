import os
import subprocess

# Настройки
OUTPUT_FILE = "project_context.txt"
# Оставляем только текстовые файлы, полезные для анализа кода
ALLOWED_EXTENSIONS = {
    '.c', '.h', '.cpp', '.hpp', '.cc',          # C/C++
    '.py', '.sh', '.bat', 'Makefile', 'CMakeLists.txt',  # Скрипты сборки
    '.md', '.txt', '.json', '.xml', '.yaml', '.yml'      # Доки и конфиги
}

def get_git_files():
    """Получает список файлов с учетом .gitignore через команду git"""
    try:
        # --cached: индексированные файлы
        # --others: новые файлы (untracked)
        # --exclude-standard: применить правила .gitignore
        result = subprocess.run(
            ['git', 'ls-files', '--cached', '--others', '--exclude-standard'],
            capture_output=True, text=True, encoding='utf-8'
        )
        if result.returncode != 0:
            print("Внимание: Это не git-репозиторий или git не найден. Использую обычный обход.")
            return None
        
        files = result.stdout.splitlines()
        # Фильтруем по расширениям
        return [f for f in files if any(f.endswith(ext) or f.split('/')[-1] in ALLOWED_EXTENSIONS for ext in ALLOWED_EXTENSIONS)]
    except Exception as e:
        print(f"Ошибка при вызове git: {e}")
        return None

def fallback_walk():
    """Запасной вариант, если git не сработал"""
    file_list = []
    for root, dirs, files in os.walk("."):
        if '.git' in dirs: dirs.remove('.git') # Игнорируем папку .git
        
        for file in files:
            if any(file.endswith(ext) or file in ALLOWED_EXTENSIONS for ext in ALLOWED_EXTENSIONS):
                full_path = os.path.join(root, file)
                # Убираем ./ в начале для красоты
                file_list.append(os.path.relpath(full_path, ".").replace("\\", "/"))
    return file_list

def generate_tree_string(file_list):
    """Строит визуальное дерево файлов из списка путей"""
    tree_str = "PROJECT STRUCTURE (Respecting .gitignore):\n==========================================\n"
    # Просто сортируем список для понятности, полноценное дерево рисовать текстом сложно без вложенных циклов,
    # но отсортированный список путей отлично читается моделью.
    last_dir = ""
    for filepath in sorted(file_list):
        current_dir = os.path.dirname(filepath)
        if current_dir != last_dir:
            tree_str += f"\n[{current_dir if current_dir else 'ROOT'}]\n"
            last_dir = current_dir
        filename = os.path.basename(filepath)
        tree_str += f"  ├── {filename}\n"
    return tree_str

def create_dump():
    print("Сбор списка файлов с учетом .gitignore...")
    files = get_git_files()
    
    if files is None:
        files = fallback_walk()
        
    print(f"Найдено файлов: {len(files)}")
    
    with open(OUTPUT_FILE, 'w', encoding='utf-8') as outfile:
        # 1. Записываем структуру
        outfile.write(generate_tree_string(files))
        outfile.write("\n\nFILE CONTENTS:\n==========================================\n")
        
        # 2. Записываем содержимое
        for filepath in files:
            if not os.path.exists(filepath): continue
            
            # Заголовок для каждого файла, чтобы я (Gemini) четко видел границы
            header = f"\n\n{'='*50}\nFILE START: {filepath}\n{'='*50}\n"
            outfile.write(header)
            
            try:
                with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
                    content = f.read()
                    if not content.strip():
                        outfile.write("(File is empty)")
                    else:
                        outfile.write(content)
            except Exception as e:
                outfile.write(f"Error reading file: {e}")

    print(f"Готово! Файл создан: {OUTPUT_FILE}")
    print("Теперь просто перетащите этот файл в чат.")

if __name__ == "__main__":
    create_dump()