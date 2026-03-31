import re

with open("仅供参考-不可修改/UI界面风格样式-旧版本.md", "r", encoding="utf-8") as f:
    content = f.read()

def extract_file(filename):
    pattern = rf"## 文件: `{re.escape(filename)}`[\s\S]*?```cpp([\s\S]*?)```"
    match = re.search(pattern, content)
    if match:
        return match.group(1).strip()
    return None

icon_helper_h = extract_file("src/ui/IconHelper.h")
string_utils_h = extract_file("src/ui/StringUtils.h")

if icon_helper_h:
    with open("IconHelper.h.tmp", "w", encoding="utf-8") as f:
        f.write(icon_helper_h)
    print("Extracted IconHelper.h")

if string_utils_h:
    with open("src/ui/StringUtils.h.tmp", "w", encoding="utf-8") as f:
        f.write(string_utils_h)
    print("Extracted StringUtils.h")
