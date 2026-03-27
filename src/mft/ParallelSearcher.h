#ifndef PARALLEL_SEARCHER_H
#define PARALLEL_SEARCHER_H

#include "mft/MftReader.h"
#include <vector>
#include <string>
#include <execution>
#include <algorithm>
#include <mutex>

namespace ArcMeta {

class ParallelSearcher {
public:
    static std::vector<DWORDLONG> search(const FileIndex& index, const std::wstring& query) {
        std::vector<const FileEntry*> entries;
        entries.reserve(index.size());
        for (auto it = index.begin(); it != index.end(); ++it) {
            entries.push_back(&it->second);
        }

        std::vector<DWORDLONG> results;
        std::mutex resultsMutex;

        std::wstring queryLower = query;
        std::transform(queryLower.begin(), queryLower.end(), queryLower.begin(), ::towlower);

        // 使用 C++17 并行算法加速搜索
        std::for_each(std::execution::par, entries.begin(), entries.end(), [&](const FileEntry* entry) {
            std::wstring nameLower = entry->name;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::towlower);

            if (nameLower.find(queryLower) != std::wstring::npos) {
                std::lock_guard<std::mutex> lock(resultsMutex);
                results.push_back(entry->frn);
            }
        });

        return results;
    }
};

} // namespace ArcMeta

#endif // PARALLEL_SEARCHER_H
