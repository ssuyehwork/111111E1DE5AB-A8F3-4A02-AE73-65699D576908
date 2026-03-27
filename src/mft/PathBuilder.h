#ifndef PATH_BUILDER_H
#define PATH_BUILDER_H

#include "MftReader.h"
#include <QString>

namespace ArcMeta {

class PathBuilder {
public:
    explicit PathBuilder(const FileIndex& index);

    // 根据 FRN 递归向上查找，还原完整路径
    // driveRoot 形如 "C:"
    QString getFullPath(DWORDLONG frn, const QString& driveRoot) const;

private:
    const FileIndex& m_index;

    // NTFS 根目录标识判断
    bool isRoot(DWORDLONG parentFrn) const;
};

} // namespace ArcMeta

#endif // PATH_BUILDER_H
