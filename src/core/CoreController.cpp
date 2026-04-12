#include "CoreController.h"
#include "../db/Database.h"
#include "../db/SyncEngine.h"
#include "../db/ItemRepo.h"
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
 * 2026-05-24 按照用户要求：实现混合扫描架构，由实时监听、稳定标识及离线对账共同保障。
 */
void CoreController::startSystem() {
    // 异步链式初始化
    QThreadPool::globalInstance()->start([this]() {
        qint64 startTime = QDateTime::currentMSecsSinceEpoch();
        qDebug() << "[Core] >>> 开始后台异步初始化链条 (混合扫描架构已就位) <<<";
        
        // 1. 初始化数据库元数据内存镜像 (关键：提供丝滑的 UI 首次渲染)
        setStatus("正在载入元数据缓存...", true);
        MetadataManager::instance().initFromDatabase();
        qDebug() << "[Core] [Step 1/2] 数据库元数据缓存加载完成，耗时:" << (QDateTime::currentMSecsSinceEpoch() - startTime) << "ms";

        // 2. 执行离线变更追平对账 (基于稳定标识 FRN)
        // 2026-05-24 按照用户要求：实现秒级启动对账逻辑。
        // 如果数据库非空，则禁止执行耗时的全量扫描，改为由 UsnWatcher 在后台追平离线变更。
        if (ItemRepo::count() > 0) {
            setStatus("正在追平离线变更...", true);
            qDebug() << "[Core] [Step 2/2] 数据库非空，已跳过全量扫描，转为后台增量监听模式。";
            // 此处无需阻塞调用 runFullScan，UsnWatcher 会在 MainWindow 构造后自动处理追平
        } else {
            setStatus("首次启动，正在建立索引...", true);
            qint64 scanStart = QDateTime::currentMSecsSinceEpoch();
            SyncEngine::instance().runFullScan();
            qDebug() << "[Core] [Step 2/2] 首次全量对账完成，耗时:" << (QDateTime::currentMSecsSinceEpoch() - scanStart) << "ms";
        }

        setStatus("系统就绪", false);
        qDebug() << "[Core] !!! 混合扫描架构初始化就绪，总耗时:" << (QDateTime::currentMSecsSinceEpoch() - startTime) << "ms";
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
