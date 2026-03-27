#include "MftReader.h"
#include <QDebug>
#include <vector>
#include <filesystem>
#include <QFileInfo>

namespace ArcMeta {

MftReader::MftReader() {
    m_index.reserve(1000000); // 预分配内存，减少 rehash
}

MftReader::~MftReader() {}

bool MftReader::scanVolume(const QString& volumeRoot) {
    // 转换路径格式，例如 C: -> \\.\C:
    QString volumePath = volumeRoot;
    if (volumePath.endsWith("\\")) volumePath.chop(1);
    if (volumePath.endsWith(":")) {
        volumePath = "\\\\.\\" + volumePath;
    }

    HANDLE hVolume = CreateFileW(
        (LPCWSTR)volumePath.utf16(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (hVolume == INVALID_HANDLE_VALUE) {
        qDebug() << "MftReader: 权限不足，启动降级扫描方案...";
        fallbackScan(volumeRoot);
        return true; // 即使降级也算扫描过
    }

    bool success = enumerateMft(hVolume);
    CloseHandle(hVolume);
    return success;
}

bool MftReader::enumerateMft(HANDLE hVolume) {
    USN_JOURNAL_DATA_V0 journalData;
    DWORD bytesRead;

    // 1. 获取 USN Journal 状态
    if (!DeviceIoControl(hVolume, FSCTL_QUERY_USN_JOURNAL, NULL, 0, &journalData, sizeof(journalData), &bytesRead, NULL)) {
        qDebug() << "MftReader: FSCTL_QUERY_USN_JOURNAL 失败";
        return false;
    }

    // 2. 准备枚举 MFT 记录
    MFT_ENUM_DATA_V0 enumData;
    enumData.StartFileReferenceNumber = 0;
    enumData.LowUsn = 0;
    enumData.HighUsn = journalData.NextUsn;

    // 缓冲区大小设为 64KB
    const int BUF_SIZE = 64 * 1024;
    std::vector<BYTE> buffer(BUF_SIZE);

    while (DeviceIoControl(hVolume, FSCTL_ENUM_USN_DATA, &enumData, sizeof(enumData), buffer.data(), BUF_SIZE, &bytesRead, NULL)) {
        if (bytesRead < sizeof(USN)) break;

        BYTE* ptr = buffer.data() + sizeof(USN); // 第一个 USN 是后续读取的起始点
        BYTE* end = buffer.data() + bytesRead;

        while (ptr < end) {
            PUSN_RECORD_V2 record = (PUSN_RECORD_V2)ptr;

            // 提取核心字段
            FileEntry entry;
            entry.frn = record->FileReferenceNumber;
            entry.parentFrn = record->ParentFileReferenceNumber;
            entry.attributes = record->FileAttributes;
            entry.name = std::wstring(record->FileName, record->FileNameLength / sizeof(WCHAR));

            // 过滤掉我们自己生成的元数据文件名
            if (entry.name != L".am_meta.json" && entry.name != L".am_meta.json.tmp") {
                m_index[entry.frn] = entry;
            }

            ptr += record->RecordLength;
        }

        // 更新起始位置，继续下一轮读取
        enumData.StartFileReferenceNumber = *(DWORDLONG*)buffer.data();
    }

    qDebug() << "MftReader: MFT 枚举完成，索引条目数:" << m_index.size();
    return true;
}

void MftReader::fallbackScan(const QString& volumeRoot) {
    std::wstring root = volumeRoot.toStdWString();
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
            FileEntry fe;
            // 注意：降级模式下无法直接获得 NTFS FRN，此处使用 std 的简易哈希模拟 ID
            // 警告：此模式下跨路径追踪能力会受限，符合文档降级预期
            fe.frn = std::hash<std::string>{}(entry.path().string());
            fe.name = entry.path().filename().wstring();
            fe.attributes = entry.is_directory() ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
            // 降级模式无法通过 ID 溯源父目录，仅填充基础信息
            m_index[fe.frn] = fe;
        }
    } catch (...) {
        qDebug() << "MftReader: 降级扫描发生异常";
    }
}

} // namespace ArcMeta
