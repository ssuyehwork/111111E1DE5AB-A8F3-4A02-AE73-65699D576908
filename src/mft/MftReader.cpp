#include "MftReader.h"
#include <algorithm>

namespace ArcMeta {

MftReader::MftReader() {
    m_index.reserve(1000000);
}

MftReader::~MftReader() {}

bool MftReader::buildIndexStub() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_index.clear();

    // 2026-03-27 模拟 MFT 数据输入，验证 visibleChildCount 计算
    // 模拟一个 Root (5), 一个空目录 (101), 一个非空目录 (102), 一个仅含隐藏文件的目录 (103)

    auto add = [&](DWORDLONG f, DWORDLONG p, const std::wstring& n, DWORD a) {
        FileEntry e; e.frn = f; e.parentFrn = p; e.name = n; e.attributes = a;
        m_index[f] = std::move(e);
    };

    add(5, 0, L"C:", FILE_ATTRIBUTE_DIRECTORY);
    add(101, 5, L"EmptyDir", FILE_ATTRIBUTE_DIRECTORY);
    add(102, 5, L"FullDir", FILE_ATTRIBUTE_DIRECTORY);
    add(201, 102, L"file.txt", FILE_ATTRIBUTE_NORMAL);
    add(103, 5, L"MetaDir", FILE_ATTRIBUTE_DIRECTORY);
    add(202, 103, L".am_meta.json", FILE_ATTRIBUTE_HIDDEN); // 隐藏文件不应被计入

    calculateChildCounts();
    return true;
}

void MftReader::calculateChildCounts() {
    // 2026-03-27 按照用户要求：必须考虑文件与父文件夹的从属上下文关系
    for (auto& pair : m_index) {
        auto& entry = pair.second;
        if (entry.isHidden()) continue; // 排除隐藏项

        auto it = m_index.find(entry.parentFrn);
        if (it != m_index.end() && it->second.isDir()) {
            it->second.visibleChildCount++;
        }
    }
}

} // namespace ArcMeta
