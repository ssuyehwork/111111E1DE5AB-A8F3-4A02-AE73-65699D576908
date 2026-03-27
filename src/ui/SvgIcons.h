#pragma once

#include <QString>
#include <QMap>

namespace ArcMeta {

/**
 * @brief 内联 SVG 图标库
 */
struct SvgIcons {
    // 2026-03-27 按照文档：禁止使用 Emoji，必须且只能使用 SVG 图标
    static inline const QMap<QString, QString> icons = {
        {"folder", R"(<svg width="64" height="64" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z"></path></svg>)"},
        {"file", R"(<svg width="64" height="64" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M13 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V9z"></path><polyline points="13 2 13 9 20 9"></polyline></svg>)"}
    };
};

} // namespace ArcMeta
