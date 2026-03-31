import re

with open("仅供参考-不可修改/UI界面风格样式-旧版本.md", "r", encoding="utf-8") as f:
    content = f.read()

def extract_file(filename):
    pattern = rf"## 文件: `{re.escape(filename)}`[\s\S]*?```cpp([\s\S]*?)```"
    match = re.search(pattern, content)
    if match:
        return match.group(1).strip()
    return None

qp_h = extract_file("src/ui/QuickPreview.h")

if qp_h:
    with open("src/ui/QuickPreview.h", "w", encoding="utf-8") as f:
        f.write(qp_h)
    print("Created QuickPreview.h")
