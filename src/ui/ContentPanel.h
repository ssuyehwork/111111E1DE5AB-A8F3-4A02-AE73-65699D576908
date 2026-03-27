#pragma once

#include <QWidget>
#include <QListView>
#include <QStandardItemModel>
#include "mft/MftReader.h"

namespace ArcMeta {

class ContentPanel : public QWidget {
    Q_OBJECT
public:
    explicit ContentPanel(QWidget* parent = nullptr);
    ~ContentPanel();

    void updateItems(const std::vector<const FileEntry*>& entries, const QString& currentPath);

protected:
    // 2026-03-27 按照宪法：新增事件过滤器以拦截原生 Tip
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QListView* m_listView = nullptr;
    QStandardItemModel* m_model = nullptr;
};

} // namespace ArcMeta
