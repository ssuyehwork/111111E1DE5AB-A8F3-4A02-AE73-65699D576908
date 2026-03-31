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

# Get all filenames from the markdown
all_files = re.findall(r"## 文件: `(.*?)`", content)

for filename in all_files:
    if filename.startswith("src/") or filename == "CMakeLists.txt":
        code = extract_file(filename)
        if code:
            os.makedirs(os.path.dirname(filename), exist_ok=True) if os.path.dirname(filename) else None
            with open(filename, "w", encoding="utf-8") as f:
                f.write(code)
            print(f"Restored {filename}")
