#ifndef FILTER_ENGINE_H
#define FILTER_ENGINE_H

#include <QStringList>
#include <QSet>

namespace ArcMeta {

struct FilterState {
    QSet<int> ratings;
    QStringList colors;
    QStringList tags;
    bool onlyPinned = false;
    bool onlyEncrypted = false;
};

class FilterEngine {
public:
    static bool match(const FilterState& state, int rating, const QString& color,
                     const QStringList& tags, bool pinned, bool encrypted) {

        // 星级匹配
        if (!state.ratings.isEmpty() && !state.ratings.contains(rating)) return false;

        // 颜色匹配
        if (!state.colors.isEmpty() && !state.colors.contains(color)) return false;

        // 标签匹配 (逻辑：包含任意一个选中的标签)
        if (!state.tags.isEmpty()) {
            bool tagFound = false;
            for (const auto& t : state.tags) {
                if (tags.contains(t)) {
                    tagFound = true;
                    break;
                }
            }
            if (!tagFound) return false;
        }

        // 置顶匹配
        if (state.onlyPinned && !pinned) return false;

        // 加密匹配
        if (state.onlyEncrypted && !encrypted) return false;

        return true;
    }
};

} // namespace ArcMeta

#endif // FILTER_ENGINE_H
