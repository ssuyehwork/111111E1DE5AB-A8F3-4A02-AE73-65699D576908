import re

with open("仅供参考-不可修改/UI界面风格样式-旧版本.md", "r", encoding="utf-8") as f:
    content = f.read()

def extract_file(filename):
    pattern = rf"## 文件: `{re.escape(filename)}`[\s\S]*?```cpp([\s\S]*?)```"
    match = re.search(pattern, content)
    if match:
        return match.group(1).strip()
    return None

search_line_edit_cpp = extract_file("src/ui/SearchLineEdit.cpp")
search_line_edit_h = extract_file("src/ui/SearchLineEdit.h")

if search_line_edit_cpp:
    with open("src/ui/SearchLineEdit.cpp", "w", encoding="utf-8") as f:
        f.write(search_line_edit_cpp)
    print("Created SearchLineEdit.cpp")

if search_line_edit_h:
    with open("src/ui/SearchLineEdit.h", "w", encoding="utf-8") as f:
        f.write(search_line_edit_h)
    print("Created SearchLineEdit.h")
