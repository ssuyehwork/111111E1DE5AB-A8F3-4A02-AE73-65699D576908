#pragma once

#include <QWidget>
#include <QListWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QDragEnterEvent>
#include <QDropEvent>

namespace ArcMeta {

/**
 * @brief 收藏面板（面板三）
 * 包含：分组标题、收藏列表、拖拽放置区
 */
class FavoritesPanel : public QWidget {
    Q_OBJECT

public:
    explicit FavoritesPanel(QWidget* parent = nullptr);
    ~FavoritesPanel() override = default;

    /**
     * @brief 添加一个收藏项
     * @param icon 系统图标
     * @param name 显示名称
     * @param path 物理路径
     */
    void addFavoriteItem(const QIcon& icon, const QString& name, const QString& path);

signals:
    /**
     * @brief 当用户点击收藏项时发出
     */
    void favoriteSelected(const QString& path);

protected:
    // 拖拽处理
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void initUi();
    void initDropZone();
    
    QVBoxLayout* m_mainLayout = nullptr;
    QListWidget* m_listWidget = nullptr;
    
    // 底部拖拽区域
    QWidget* m_dropZone = nullptr;
    QLabel* m_dropLabel = nullptr;

private slots:
    void onItemClicked(QListWidgetItem* item);
    void onCustomContextMenuRequested(const QPoint& pos);
};

} // namespace ArcMeta
