#pragma once

#include <QWidget>
#include <QTreeView>
#include <QFileSystemModel>
#include <QIdentityProxyModel>

namespace ArcMeta {

class NavProxyModel : public QIdentityProxyModel {
    Q_OBJECT
public:
    using QIdentityProxyModel::QIdentityProxyModel;
    QVariant data(const QModelIndex& index, int role) const override;
};

class NavPanel : public QWidget {
    Q_OBJECT
public:
    explicit NavPanel(QWidget* parent = nullptr);
    ~NavPanel();

protected:
    // 2026-03-27 按照宪法：新增事件过滤器以拦截原生 Tip
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QTreeView* m_treeView = nullptr;
    QFileSystemModel* m_fsModel = nullptr;
    NavProxyModel* m_proxyModel = nullptr;
};

} // namespace ArcMeta
