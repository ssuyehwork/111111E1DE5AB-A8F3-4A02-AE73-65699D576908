import re

with open("仅供参考-不可修改/UI界面风格样式-旧版本.md", "r", encoding="utf-8") as f:
    content = f.read()

def extract_file(filename):
    pattern = rf"## 文件: `{re.escape(filename)}`[\s\S]*?```cpp([\s\S]*?)```"
    match = re.search(pattern, content)
    if match:
        return match.group(1).strip()
    return None

files_to_extract = [
    "src/ui/CategoryLockWidget.h",
    "src/ui/CategoryLockWidget.cpp",
    "src/ui/NoteEditWindow.h",
    "src/ui/NoteEditWindow.cpp",
    "src/ui/Editor.h",
    "src/ui/Editor.cpp"
]

for filename in files_to_extract:
    code = extract_file(filename)
    if code:
        with open(filename, "w", encoding="utf-8") as f:
            f.write(code)
        print(f"Restored {filename}")
