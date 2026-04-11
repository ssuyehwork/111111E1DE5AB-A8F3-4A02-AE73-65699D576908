#include "CoreController.h"
#include "../db/Database.h"
#include "../db/SyncEngine.h"
#include "../meta/SyncQueue.h"
#include "../meta/MetadataManager.h"
#include <QtConcurrent>
#include <QThreadPool>
#include <QThread>
#include <QDateTime>
#include <QDebug>

namespace ArcMeta {

CoreController& CoreController::instance() {
    static CoreController inst;
    return inst;
}

CoreController::CoreController(QObject* parent) : QObject(parent) {}

CoreController::~CoreController() {}

/**
 * @brief 启动系统初始化链条
 * 2026-03-xx 按照用户最新要求：彻底移除 MFT 引擎构建，仅加载数据库、配置及增量 JSON。
 */
void CoreController::startSystem() {
    // 异步链式初始化
    QThreadPool::globalInstance()->start([this]() {
        qint64 startTime = QDateTime::currentMSecsSinceEpoch();
        qDebug() << "[Core] >>> 开始后台异步初始化链条 (MFT 已移除) <<<";
        
        // 1. 初始化数据库元数据内存镜像 (关键：消除 UI 启动后的 IO 抖动)
        setStatus("正在载入元数据缓存...", true);
        MetadataManager::instance().initFromDatabase();
        qDebug() << "[Core] [Step 1/3] 数据库元数据缓存加载完成，耗时:" << (QDateTime::currentMSecsSinceEpoch() - startTime) << "ms";

        // 2. 启动同步队列
        setStatus("启动同步引擎...", true);
        qint64 syncQueueStart = QDateTime::currentMSecsSinceEpoch();
        SyncQueue::instance().start();
        qDebug() << "[Core] [Step 2/3] 同步引擎启动完成，耗时:" << (QDateTime::currentMSecsSinceEpoch() - syncQueueStart) << "ms";

        // 3. 执行增量同步 (基于分布式 JSON 对齐元数据)
        setStatus("正在校验增量数据...", true);

        // 2026-05-20 性能优化：增量同步移出关键初始化路径，允许后台静默执行
        // 并显式设置线程池优先级，确保不干扰 UI 响应。使用 QThreadPool 避免 QFuture 忽略警告。
        QThreadPool::globalInstance()->start([]() {
            QThread::currentThread()->setPriority(QThread::LowPriority);
            SyncEngine::instance().runIncrementalSync();
        });

        qDebug() << "[Core] [Step 3/3] 增量同步已异步挂起";

        setStatus("系统就绪", false);
        qDebug() << "[Core] !!! 所有初始化任务已就绪，总耗时:" << (QDateTime::currentMSecsSinceEpoch() - startTime) << "ms，正在发射信号...";
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
