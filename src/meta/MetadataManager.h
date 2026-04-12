#ifndef ARCMETA_METADATA_MANAGER_H
#define ARCMETA_METADATA_MANAGER_H

#include "MetadataDefs.h"
#include <QObject>
#include <QString>
#include <QTimer>
#include <QStringList>
#include <unordered_map>
#include <shared_mutex>
#include <string>

namespace ArcMeta {

/**
 * @brief 内存元数据镜像结构
 */
struct RuntimeMeta {
    int rating = 0;
    std::wstring color;
    QStringList tags;
    std::wstring note;
    bool pinned = false;
    bool encrypted = false;
};

/**
 * @brief 元数据管理器
 */
class MetadataManager : public QObject {
    Q_OBJECT
public:
    static MetadataManager& instance();

    void initFromDatabase();
    RuntimeMeta getMeta(const std::wstring& path);
    void prefetchDirectory(const std::wstring& dirPath);

    void setRating(const std::wstring& path, int rating);
    void setColor(const std::wstring& path, const std::wstring& color);
    void setPinned(const std::wstring& path, bool pinned);
    void setTags(const std::wstring& path, const QStringList& tags);
    void setNote(const std::wstring& path, const std::wstring& note);
    void setEncrypted(const std::wstring& path, bool encrypted);

    void renameItem(const std::wstring& oldPath, const std::wstring& newPath);

signals:
    void metaChanged(const std::wstring& path);

private:
    MetadataManager(QObject* parent = nullptr);
    ~MetadataManager() override = default;

    std::unordered_map<std::wstring, RuntimeMeta> m_cache;
    mutable std::shared_mutex m_mutex;
    std::unordered_map<std::wstring, QTimer*> m_debounceTimers;

    void persistAsync(const std::wstring& path);
    void debouncePersist(const std::wstring& path);
};

} // namespace ArcMeta

#endif // ARCMETA_METADATA_MANAGER_H
