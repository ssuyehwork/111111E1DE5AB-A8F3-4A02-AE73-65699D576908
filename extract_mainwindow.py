import re

with open("仅供参考-不可修改/UI界面风格样式-旧版本.md", "r", encoding="utf-8") as f:
    content = f.read()

def extract_file(filename):
    pattern = rf"## 文件: `{re.escape(filename)}`[\s\S]*?```cpp([\s\S]*?)```"
    match = re.search(pattern, content)
    if match:
        return match.group(1).strip()
    return None

mw_cpp = extract_file("src/ui/MainWindow.cpp")
mw_h = extract_file("src/ui/MainWindow.h")

if mw_cpp:
    with open("MainWindow.cpp.old", "w", encoding="utf-8") as f:
        f.write(mw_cpp)
    print("Extracted MainWindow.cpp.old")

if mw_h:
    with open("MainWindow.h.old", "w", encoding="utf-8") as f:
        f.write(mw_h)
    print("Extracted MainWindow.h.old")
