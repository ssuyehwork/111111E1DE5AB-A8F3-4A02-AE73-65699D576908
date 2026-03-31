import re

with open("仅供参考-不可修改/UI界面风格样式-旧版本.md", "r", encoding="utf-8") as f:
    content = f.read()

pattern = r"## 文件: `src/main.cpp`[\s\S]*?```cpp([\s\S]*?)```"
match = re.search(pattern, content)
if match:
    code = match.group(1).strip()
    with open("src/main.cpp", "w", encoding="utf-8") as f:
        f.write(code)
    print("Fixed src/main.cpp")
