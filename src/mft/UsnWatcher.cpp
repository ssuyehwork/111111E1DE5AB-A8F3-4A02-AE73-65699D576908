#include "UsnWatcher.h"
#include "PathBuilder.h"
#include "../db/ItemRepo.h"
#include <QDebug>
#include <vector>

namespace ArcMeta {

UsnWatcher::UsnWatcher(FileIndex& index, const QString& volumeRoot, QObject* parent)
    : QThread(parent), m_index(index), m_volumeRoot(volumeRoot) {
}

UsnWatcher::~UsnWatcher() {
    stop();
}

void UsnWatcher::stop() {
    m_isRunning = false;
    if (m_hVolume != INVALID_HANDLE_VALUE) {
        CancelIoEx(m_hVolume, NULL);
    }
    wait();
}

HANDLE UsnWatcher::openVolume(const QString& volumeRoot) {
    QString volumePath = volumeRoot;
    if (volumePath.endsWith("\\")) volumePath.chop(1);
    if (volumePath.endsWith(":")) {
        volumePath = "\\\\.\\" + volumePath;
    }

    return CreateFileW(
        (LPCWSTR)volumePath.utf16(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );
}

bool UsnWatcher::initJournal() {
    m_hVolume = openVolume(m_volumeRoot);
    if (m_hVolume == INVALID_HANDLE_VALUE) {
        qDebug() << "UsnWatcher: 无法打开卷句柄";
        return false;
    }

    DWORD bytesRead;
    if (!DeviceIoControl(m_hVolume, FSCTL_QUERY_USN_JOURNAL, NULL, 0, &m_journalData, sizeof(m_journalData), &bytesRead, NULL)) {
        qDebug() << "UsnWatcher: 无法查询 USN Journal";
        return false;
    }

    return true;
}

void UsnWatcher::run() {
    if (!initJournal()) return;
    watchLoop();
    if (m_hVolume != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hVolume);
        m_hVolume = INVALID_HANDLE_VALUE;
    }
}

void UsnWatcher::watchLoop() {
    READ_USN_JOURNAL_DATA_V0 readData;
    readData.StartUsn = m_journalData.NextUsn;
    readData.ReasonMask = USN_REASON_FILE_CREATE | USN_REASON_FILE_DELETE |
                         USN_REASON_RENAME_OLD_NAME | USN_REASON_RENAME_NEW_NAME |
                         USN_REASON_CLOSE;
    readData.ReturnOnlyOnClose = FALSE;
    readData.Timeout = 0;
    readData.BytesToWaitFor = 0;
    readData.UsnJournalID = m_journalData.UsnJournalID;

    const int BUF_SIZE = 64 * 1024;
    std::vector<BYTE> buffer(BUF_SIZE);
    DWORD bytesRead;

    while (m_isRunning) {
        if (DeviceIoControl(m_hVolume, FSCTL_READ_USN_JOURNAL, &readData, sizeof(readData), buffer.data(), BUF_SIZE, &bytesRead, NULL)) {
            if (bytesRead < sizeof(USN)) {
                msleep(200);
                continue;
            }

            BYTE* ptr = buffer.data() + sizeof(USN);
            BYTE* end = buffer.data() + bytesRead;

            while (ptr < end) {
                PUSN_RECORD_V2 record = (PUSN_RECORD_V2)ptr;
                handleRecord(record);
                ptr += record->RecordLength;
            }

            readData.StartUsn = *(USN*)buffer.data();
        } else {
            DWORD error = GetLastError();
            if (error == ERROR_OPERATION_ABORTED) break;
            qDebug() << "UsnWatcher: 读取错误" << error;
            msleep(1000);
        }
    }
}

void UsnWatcher::handleRecord(PUSN_RECORD_V2 record) {
    DWORD reason = record->Reason;
    DWORDLONG frn = record->FileReferenceNumber;
    std::wstring fileName(record->FileName, record->FileNameLength / sizeof(WCHAR));

    if (fileName == L".am_meta.json" || fileName == L".am_meta.json.tmp") return;

    if (reason & USN_REASON_FILE_CREATE) {
        FileEntry entry;
        entry.frn = frn;
        entry.parentFrn = record->ParentFileReferenceNumber;
        entry.attributes = record->FileAttributes;
        entry.name = fileName;
        m_index[frn] = entry;
    }
    else if (reason & USN_REASON_FILE_DELETE) {
        m_index.erase(frn);
        // 级联清理数据库记录
        ItemRepo::removeByFrn(frn);
        emit fileDeleted(frn, "");
    }
    else if (reason & USN_REASON_RENAME_NEW_NAME) {
        if (m_index.count(frn)) {
            m_index[frn].name = fileName;
            m_index[frn].parentFrn = record->ParentFileReferenceNumber;
        } else {
            FileEntry entry;
            entry.frn = frn;
            entry.parentFrn = record->ParentFileReferenceNumber;
            entry.attributes = record->FileAttributes;
            entry.name = fileName;
            m_index[frn] = entry;
        }

        // 核心：元数据自动追踪
        // 利用 PathBuilder 重建新路径并更新数据库
        PathBuilder builder(m_index);
        QString newPath = builder.getFullPath(frn, m_volumeRoot);

        // 获取父目录路径
        DWORDLONG parentFrn = record->ParentFileReferenceNumber;
        QString newParentPath = builder.getFullPath(parentFrn, m_volumeRoot);

        if (ItemRepo::updatePath(frn, newPath, newParentPath)) {
            qDebug() << "UsnWatcher: 成功追踪元数据移动至 -" << newPath;
        }

        emit fileChanged(frn, newPath, reason);
    }
}

} // namespace ArcMeta
