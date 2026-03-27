#ifndef SYNC_ENGINE_H
#define SYNC_ENGINE_H

#include <QObject>
#include <QStringList>
#include <functional>

namespace ArcMeta {

class SyncEngine : public QObject {
    Q_OBJECT
public:
    explicit SyncEngine(QObject* parent = nullptr);

    // 全量扫描：递归遍历目录，同步所有发现的 .am_meta.json
    void fullScan(const QString& rootPath, std::function<void(int current, int total)> progressCallback = nullptr);

    // 标签聚合：从 items 和 folders 表重新统计所有标签出现次数
    void rebuildTagIndex();

public slots:
    // 处理懒更新队列发来的同步请求
    void onSyncRequested(const QStringList& dirPaths);

private:
    // 解析单个 JSON 文件并同步到数据库
    bool syncDirectory(const QString& dirPath);
};

} // namespace ArcMeta

#endif // SYNC_ENGINE_H
