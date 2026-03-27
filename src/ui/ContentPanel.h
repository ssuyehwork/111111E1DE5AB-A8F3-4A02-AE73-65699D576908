#pragma once

#include <QWidget>
#include <QListView>
#include <QStandardItemModel>
#include "mft/MftReader.h"

namespace ArcMeta {

/**
 * @brief 内容面板，显示当前目录下的条目
 */
class ContentPanel : public QWidget {
    Q_OBJECT
public:
    explicit ContentPanel(QWidget* parent = nullptr);
    ~ContentPanel();

    /**
     * @brief 刷新视图
     * 2026-03-27 此处体现上下文：将 MFT 的空状态映射到原生图标滤镜
     */
    void updateItems(const std::vector<const FileEntry*>& entries, const QString& currentPath);

private:
    QListView* m_listView = nullptr;
    QStandardItemModel* m_model = nullptr;
};

} // namespace ArcMeta
