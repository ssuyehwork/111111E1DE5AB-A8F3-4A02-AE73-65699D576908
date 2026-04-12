#include "CoreController.h"
#include "../meta/SyncQueue.h"
#include "../meta/MetadataManager.h"
#include <QtConcurrent>
#include <QThreadPool>
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
        qDebug() << "[Core] >>> 开始后台异步初始化链条 (纯 JSON 模式) <<<";
        
        // 1. 初始化元数据内存镜像 (2026-05-22 数据库已废除)
        setStatus("正在准备元数据服务...", true);
        MetadataManager::instance().initFromDatabase();
        qDebug() << "[Core] [Step 1/2] 元数据服务准备完成，耗时:" << (QDateTime::currentMSecsSinceEpoch() - startTime) << "ms";

        // 2. 启动同步队列 (仅保留异步 IO 管理功能)
        setStatus("启动异步服务...", true);
        qint64 syncQueueStart = QDateTime::currentMSecsSinceEpoch();
        SyncQueue::instance().start();
        qDebug() << "[Core] [Step 2/2] 异步服务启动完成，耗时:" << (QDateTime::currentMSecsSinceEpoch() - syncQueueStart) << "ms";

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
