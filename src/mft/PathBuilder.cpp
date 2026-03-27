#include "PathBuilder.h"
#include <vector>
#include <algorithm>

namespace ArcMeta {

PathBuilder::PathBuilder(const FileIndex& index) : m_index(index) {}

QString PathBuilder::getFullPath(DWORDLONG frn, const QString& driveRoot) const {
    std::vector<std::wstring> pathParts;
    DWORDLONG currentFrn = frn;
    int depth = 0;
    const int MAX_DEPTH = 64; // 防止循环引用导致的死循环

    while (depth < MAX_DEPTH) {
        auto it = m_index.find(currentFrn);
        if (it == m_index.end()) {
            // 索引中找不到记录，可能是扫描后删除的文件或特殊文件
            break;
        }

        const FileEntry& entry = it->second;
        pathParts.push_back(entry.name);

        if (isRoot(entry.parentFrn)) {
            // 已到达卷根目录
            break;
        }

        currentFrn = entry.parentFrn;
        depth++;
    }

    // 将路径片段反转并拼接
    std::reverse(pathParts.begin(), pathParts.end());

    QString fullPath = driveRoot;
    if (!fullPath.endsWith("\\")) fullPath += "\\";

    for (size_t i = 0; i < pathParts.size(); ++i) {
        fullPath += QString::fromStdWString(pathParts[i]);
        if (i < pathParts.size() - 1) {
            fullPath += "\\";
        }
    }

    return fullPath;
}

bool PathBuilder::isRoot(DWORDLONG parentFrn) const {
    // 根据 ArcMeta.md 规范，根目录判断条件
    return (parentFrn & 0x0000FFFFFFFFFFFF) == 5;
}

} // namespace ArcMeta
