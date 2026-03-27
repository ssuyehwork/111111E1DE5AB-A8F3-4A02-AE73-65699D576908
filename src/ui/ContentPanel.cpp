#include "ContentPanel.h"
#include "IconHelper.h"
#include <QVBoxLayout>
#include <QStandardItem>
#include <QDir>

namespace ArcMeta {

ContentPanel::ContentPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_listView = new QListView(this);
    m_listView->setViewMode(QListView::IconMode);
    m_listView->setIconSize(QSize(64, 64));
    m_listView->setGridSize(QSize(110, 100));
    m_listView->setSpacing(10);

    m_model = new QStandardItemModel(this);
    m_listView->setModel(m_model);

    layout->addWidget(m_listView);
}

ContentPanel::~ContentPanel() {}

void ContentPanel::updateItems(const std::vector<const FileEntry*>& entries, const QString& currentPath) {
    m_model->clear();

    for (const auto* entry : entries) {
        QString name = QString::fromStdWString(entry->name);
        auto* item = new QStandardItem(name);

        if (entry->isDir()) {
            // 2026-03-27 获取该文件夹的完整物理路径（用于抓取原生图标）
            QString fullPath = QDir(currentPath).filePath(name);

            // [逻辑整合] 2026-03-27 基于 MFT 计数决定是否应用银灰色滤镜
            item->setIcon(IconHelper::getFolderIcon(fullPath, entry->isEmptyDir()));
        } else {
            // 文件使用原生图标
            QFileIconProvider provider;
            item->setIcon(provider.icon(QFileInfo(QDir(currentPath).filePath(name))));
        }

        m_model->appendRow(item);
    }
}

} // namespace ArcMeta
