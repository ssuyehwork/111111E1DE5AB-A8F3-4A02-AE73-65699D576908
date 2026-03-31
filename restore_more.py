import re
import os

with open("仅供参考-不可修改/UI界面风格样式-旧版本.md", "r", encoding="utf-8") as f:
    content = f.read()

def extract_file(filename):
    pattern = rf"## 文件: `{re.escape(filename)}`[\s\S]*?```cpp([\s\S]*?)```"
    match = re.search(pattern, content)
    if match:
        return match.group(1).strip()
    return None

files_to_restore = [
    "src/core/DatabaseManager.h",
    "src/core/DatabaseManager.cpp",
    "src/ui/MetadataPanel.h",
    "src/ui/MetadataPanel.cpp",
    "src/ui/FilterPanel.h",
    "src/ui/FilterPanel.cpp",
    "src/ui/DropTreeView.h",
    "src/ui/DropTreeView.cpp"
]

for filename in files_to_restore:
    code = extract_file(filename)
    if code:
        os.makedirs(os.path.dirname(filename), exist_ok=True)
        with open(filename, "w", encoding="utf-8") as f:
            f.write(code)
        print(f"Restored {filename}")
    else:
        print(f"Could not find {filename} in markdown")
