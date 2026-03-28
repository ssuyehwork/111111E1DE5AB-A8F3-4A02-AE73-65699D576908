#pragma once

#include <QWidget>
#include <QTreeView>
#include <QFileSystemModel>
#include <QVBoxLayout>

namespace ArcMeta {

/**
 * @brief 导航面板（面板二）
 * 使用 QTreeView + QFileSystemModel 实现文件夹树导航
 */
class NavPanel : public QWidget {
    Q_OBJECT

public:
    explicit NavPanel(QWidget* parent = nullptr);
    ~NavPanel() override = default;

    /**
     * @brief 设置并跳转到指定目录
     * @param path 完整路径
     */
    void setRootPath(const QString& path);

signals:
    /**
     * @brief 当用户点击目录时发出信号
     * @param path 目标目录完整路径
     */
    void directorySelected(const QString& path);

private:
    void initUi();
    
    QTreeView* m_treeView = nullptr;
    QFileSystemModel* m_fileModel = nullptr;
    QVBoxLayout* m_mainLayout = nullptr;

private slots:
    void onTreeClicked(const QModelIndex& index);
};

} // namespace ArcMeta
