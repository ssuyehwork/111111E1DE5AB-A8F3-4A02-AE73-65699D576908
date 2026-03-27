#ifndef MFT_READER_H
#define MFT_READER_H

#include <QString>
#include <QMap>
#include <unordered_map>
#include <string>
#include <windows.h>
#include <winioctl.h>

namespace ArcMeta {

struct FileEntry {
    DWORDLONG frn;         // File Reference Number
    DWORDLONG parentFrn;   // 父目录 FRN
    std::wstring name;     // 文件名
    DWORD attributes;      // 文件属性
    int visibleChildCount = 0;

    bool isDir() const { return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0; }
};

// 使用 unordered_map 以获得 O(1) 的 FRN 查找性能
using FileIndex = std::unordered_map<DWORDLONG, FileEntry>;

class MftReader {
public:
    MftReader();
    ~MftReader();

    // 执行 MFT 扫描，构建全盘索引
    bool scanVolume(const QString& volumeRoot);

    const FileIndex& getIndex() const { return m_index; }

private:
    FileIndex m_index;

    // 底层读取逻辑
    bool enumerateMft(HANDLE hVolume);
};

} // namespace ArcMeta

#endif // MFT_READER_H
