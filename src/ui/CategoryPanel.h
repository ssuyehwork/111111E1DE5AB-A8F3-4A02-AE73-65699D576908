#pragma once

#include <QWidget>
#include <QTreeView>
#include <QStandardItemModel>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QMenu>
#include <QAction>

namespace ArcMeta {

/**
 * @brief 分类面板（面板一）
 * 包含：顶部统计区、中间分类树区、底部工具栏
 */
class CategoryPanel : public QFrame {
    Q_OBJECT

public:
    explicit CategoryPanel(QWidget* parent = nullptr);
    ~CategoryPanel() override = default;

signals:
    void categorySelected(const QString& name);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    // UI 初始化方法
    void initUi();
    void initTopStats();
    void initCategoryTree();
    void initBottomToolbar();

    // 内部组件
    QVBoxLayout* m_mainLayout = nullptr;
    QWidget* m_focusLine = nullptr;
    
    // 统计项容器
    QWidget* m_statsWidget = nullptr;
    QVBoxLayout* m_statsLayout = nullptr;

    // 分类树
    QTreeView* m_treeView = nullptr;
    QStandardItemModel* m_treeModel = nullptr;

    // 底部工具栏
    QWidget* m_bottomToolbar = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QPushButton* m_addCategoryBtn = nullptr;

    // 内部私有辅助
    void addStatItem(const QString& iconKey, const QString& name, int count);
    
    // 右键菜单 Action 声明
    void setupContextMenu();
    QAction* actNewData = nullptr;
    QAction* actAssignToCategory = nullptr;
    QAction* actImportData = nullptr;
    QAction* actExport = nullptr;
    QAction* actSetColor = nullptr;
    QAction* actRandomColor = nullptr;
    QAction* actSetPresetTags = nullptr;
    QAction* actNewSibling = nullptr;
    QAction* actNewChild = nullptr;
    QAction* actTogglePin = nullptr;
    QAction* actRename = nullptr;
    QAction* actDelete = nullptr;
    QAction* actSortByName = nullptr;
    QAction* actSortByCount = nullptr;
    QAction* actSortByTime = nullptr;
    QAction* actPasswordProtect = nullptr;

private slots:
    void onCustomContextMenuRequested(const QPoint& pos);
};

} // namespace ArcMeta
