#ifndef NOTETYPES_H
#define NOTETYPES_H

#include <QString>
#include <QStringList>
#include <QDate>

// [FIX] 2026-03-xx 物理隔离基础类型定义，彻底消除由于包含顺序导致的编译塌陷
struct FilterState {
    QString keyword;
    bool inTitle = true;
    bool inContent = true;
    bool inTags = true;
    QStringList tags;
    QDate startDate;
    QDate endDate;
    bool hasAttachment = false;
    QString itemType = "all";
    int minRating = 0;
    bool onlyFavorite = false;
    bool onlyPinned = false;
};

#endif // NOTETYPES_H
