#ifndef USN_WATCHER_H
#define USN_WATCHER_H

#include <QThread>
#include <QString>
#include <windows.h>
#include <winioctl.h>
#include <atomic>
#include "MftReader.h"

namespace ArcMeta {

class UsnWatcher : public QThread {
    Q_OBJECT
public:
    explicit UsnWatcher(FileIndex& index, const QString& volumeRoot, QObject* parent = nullptr);
    ~UsnWatcher();

    void stop();

signals:
    // 当文件系统发生重大变化需要通知其他组件时
    void fileChanged(DWORDLONG frn, const QString& newPath, DWORD reason);
    void fileDeleted(DWORDLONG frn, const QString& oldPath);

protected:
    void run() override;

private:
    FileIndex& m_index;
    QString m_volumeRoot;
    std::atomic<bool> m_isRunning{true};

    USN_JOURNAL_DATA_V0 m_journalData;
    HANDLE m_hVolume = INVALID_HANDLE_VALUE;

    bool initJournal();
    void watchLoop();

    // 处理具体的记录变动
    void handleRecord(PUSN_RECORD_V2 record);

    // 获取卷句柄
    HANDLE openVolume(const QString& volumeRoot);
};

} // namespace ArcMeta

#endif // USN_WATCHER_H
