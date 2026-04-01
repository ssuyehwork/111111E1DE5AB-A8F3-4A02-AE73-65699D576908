#pragma once

#include <QFrame>
#include <QTreeView>
#include <QVBoxLayout>

namespace ArcMeta {

class CategoryModel;
class DropTreeView;

/**
 * @brief 分类面板（面板一）
 * 还原旧版双树架构
 */
class CategoryPanel : public QFrame {
    Q_OBJECT

public:
    explicit CategoryPanel(QWidget* parent = nullptr);
    ~CategoryPanel() override = default;

    /**
     * @brief 物理还原：设置 1px 翠绿高亮线的显隐状态
     */
    void setFocusHighlight(bool visible);

signals:
    void categorySelected(const QString& name);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onCreateCategory();
    void onCreateSubCategory();
    void onRenameCategory();
    void onDeleteCategory();
    void onAddData();
    void onClassifyToCategory();
    void onSetColor();
    void onRandomColor();
    void onSetPresetTags();
    void onTogglePin();
    void onSetPassword();
    void onClearPassword();

private:
    void initUi();
    void setupContextMenu();

    QVBoxLayout* m_mainLayout = nullptr;
    QWidget* m_focusLine = nullptr;
    
    DropTreeView* m_categoryTree = nullptr;
    CategoryModel* m_categoryModel = nullptr;
};

} // namespace ArcMeta
