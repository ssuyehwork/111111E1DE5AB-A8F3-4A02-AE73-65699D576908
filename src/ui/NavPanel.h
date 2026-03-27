#pragma once

#include <QWidget>
#include <QTreeView>
#include <QFileSystemModel>
#include <QIdentityProxyModel>

namespace ArcMeta {

/**
 * @brief 导航模型，负责在 Tree 视图中应用原生图标置灰逻辑
 */
class NavProxyModel : public QIdentityProxyModel {
    Q_OBJECT
public:
    using QIdentityProxyModel::QIdentityProxyModel;

    // 2026-03-27 拦截图标获取请求，实现 NavPanel 的银灰色视觉对齐
    QVariant data(const QModelIndex& index, int role) const override;
};

/**
 * @brief 导航面板
 */
class NavPanel : public QWidget {
    Q_OBJECT
public:
    explicit NavPanel(QWidget* parent = nullptr);
    ~NavPanel();

private:
    QTreeView* m_treeView = nullptr;
    QFileSystemModel* m_fsModel = nullptr;
    NavProxyModel* m_proxyModel = nullptr;
};

} // namespace ArcMeta
