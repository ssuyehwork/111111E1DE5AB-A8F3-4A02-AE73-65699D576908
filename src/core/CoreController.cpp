#include "CoreController.h"
#include "../db/Database.h"
#include "../db/SyncEngine.h"
#include "../meta/SyncQueue.h"
#include "../meta/MetadataManager.h"
#include "../mft/MftReader.h"
#include <QtConcurrent>
#include <QDebug>

namespace ArcMeta {

CoreController& CoreController::instance() {
    static CoreController inst;
    return inst;
}

CoreController::CoreController(QObject* parent) : QObject(parent) {}

CoreController::~CoreController() {}

void CoreController::startSystem() {
    // 异步链式初始化
    QtConcurrent::run([this]() {
        // 1. 初始化数据库元数据内存镜像 (关键：消除 UI 启动后的 IO 抖动)
        setStatus("正在载入元数据缓存...", true);
        MetadataManager::instance().initFromDatabase();

        // 2. 底层 MFT 索引构建 (耗时操作)
        setStatus("正在构建全盘索引...", true);
        MftReader::instance().buildIndex();

        // 3. 启动同步队列
        setStatus("启动同步引擎...", true);
        SyncQueue::instance().start();

        // 4. 执行增量同步 (耗时操作)
        setStatus("正在校验增量数据...", true);
        SyncEngine::instance().runIncrementalSync();

        setStatus("系统就绪", false);
        emit initializationFinished();
    });
}

void CoreController::setStatus(const QString& text, bool indexing) {
    if (m_statusText != text) {
        m_statusText = text;
        emit statusTextChanged(m_statusText);
    }
    if (m_isIndexing != indexing) {
        m_isIndexing = indexing;
        emit isIndexingChanged(m_isIndexing);
    }
}

} // namespace ArcMeta
