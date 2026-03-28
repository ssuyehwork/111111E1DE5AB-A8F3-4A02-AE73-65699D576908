#include "PathBuilder.h"
#include <algorithm>

namespace ArcMeta {

std::wstring PathBuilder::getPath(const std::wstring& volume, DWORDLONG frn) {
    std::unordered_set<DWORDLONG> visited;
    return resolveRecursive(volume, frn, visited, 0);
}

/**
 * @brief 递归重构路径
 * 严格红线：64层深度限制 + 环路检测 (visitedFrns)
 */
std::wstring PathBuilder::resolveRecursive(const std::wstring& volume, DWORDLONG frn,
                                             std::unordered_set<DWORDLONG>& visited, int depth) {
    // 1. 深度保护
    if (depth > 64) return L"";

    // 2. 环路检测
    if (visited.count(frn)) return L"";
    visited.insert(frn);

    // 3. 从索引获取 Entry
    auto& volumeMap = MftReader::instance().getIndex()[volume];
    if (volumeMap.find(frn) == volumeMap.end()) {
        return L""; // 索引不完整
    }

    const FileEntry& entry = volumeMap[frn];

    // 4. 根目录判断条件：(parentFrn & 0x0000FFFFFFFFFFFF) == 5
    DWORDLONG pureParentFrn = entry.parentFrn & 0x0000FFFFFFFFFFFFLL;
    if (pureParentFrn == 5) {
        return volume + L"\\" + entry.name;
    }

    // 5. 继续向上递归
    std::wstring parentPath = resolveRecursive(volume, entry.parentFrn, visited, depth + 1);
    if (parentPath.empty()) return L"";

    return parentPath + L"\\" + entry.name;
}

} // namespace ArcMeta
