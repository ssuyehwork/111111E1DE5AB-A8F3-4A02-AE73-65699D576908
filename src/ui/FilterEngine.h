#ifndef FILTER_ENGINE_H
#define FILTER_ENGINE_H

#include <QStringList>
#include <QSet>
#include <windows.h>

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

        if (!state.ratings.isEmpty() && !state.ratings.contains(rating)) return false;
        if (!state.colors.isEmpty() && !state.colors.contains(color)) return false;

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

        if (state.onlyPinned && !pinned) return false;
        if (state.onlyEncrypted && !encrypted) return false;

        return true;
    }
};

} // namespace ArcMeta

#endif // FILTER_ENGINE_H
