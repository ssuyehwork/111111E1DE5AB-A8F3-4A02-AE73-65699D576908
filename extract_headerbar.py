import re

with open("仅供参考-不可修改/UI界面风格样式-旧版本.md", "r", encoding="utf-8") as f:
    content = f.read()

def extract_file(filename):
    pattern = rf"## 文件: `{re.escape(filename)}`[\s\S]*?```cpp([\s\S]*?)```"
    match = re.search(pattern, content)
    if match:
        return match.group(1).strip()
    return None

header_bar_cpp = extract_file("src/ui/HeaderBar.cpp")
header_bar_h = extract_file("src/ui/HeaderBar.h")

if header_bar_cpp:
    with open("HeaderBar.cpp.tmp", "w", encoding="utf-8") as f:
        f.write(header_bar_cpp)
    print("Extracted HeaderBar.cpp")

if header_bar_h:
    with open("HeaderBar.h.tmp", "w", encoding="utf-8") as f:
        f.write(header_bar_h)
    print("Extracted HeaderBar.h")
