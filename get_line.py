import sys
import re

pattern = sys.argv[1]
with open("仅供参考-不可修改/UI界面风格样式-旧版本.md", "r", encoding="utf-8") as f:
    for i, line in enumerate(f, 1):
        if pattern in line:
            print(f"{i}:{line.strip()}")
