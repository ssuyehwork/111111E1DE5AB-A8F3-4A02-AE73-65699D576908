#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QVariant>
#include <QVariantList>
#include <QRecursiveMutex>
#include <QStringList>
#include <QSet>
#include <QMutex>
#include <QTimer>

class DatabaseManager : public QObject {
    Q_OBJECT
public:
    enum MoveDirection { Up, Down, Top, Bottom };
    static constexpr int DEFAULT_PAGE_SIZE = 100;
    
    static DatabaseManager& instance();

    bool init(const QString& dbPath = "manager.db");
    QString getLastError() const { return m_lastError; }
    void closeAndPack();
    
    // 核心项目（Item）操作
    int addItem(const QString& title, const QString& content, const QStringList& tags = QStringList(), 
                const QString& color = "", int categoryId = -1, 
                const QString& itemType = "file");
    bool updateItem(int id, const QString& title, const QString& content, const QStringList& tags);
    bool deleteItemsBatch(const QList<int>& ids);
    bool updateItemState(int id, const QString& column, const QVariant& value);
    
    // 分类管理 (保留分类逻辑，但泛化为文件夹包)
    int addCategory(const QString& name, int parentId = -1);
    QList<QVariantMap> getAllCategories();
    
    // 搜索与查询
    QList<QVariantMap> searchItems(const QString& keyword, int page = -1);
    QVariantMap getItemById(int id);

    // 统计
    QVariantMap getCounts();

    void beginBatch();
    void endBatch();

signals:
    void itemAdded(const QVariantMap& item);
    void itemUpdated();
    void categoriesChanged();

private:
    DatabaseManager(QObject* parent = nullptr);
    QSqlDatabase m_db;
    QString m_lastError;
    QRecursiveMutex m_mutex;
    bool m_isInitialized = false;
};

#endif // DATABASEMANAGER_H
