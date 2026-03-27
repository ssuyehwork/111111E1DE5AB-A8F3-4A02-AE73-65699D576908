#ifndef META_PANEL_H
#define META_PANEL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QCheckBox>
#include <QPlainTextEdit>
#include <QLineEdit>

namespace ArcMeta {

class MetaPanel : public QWidget {
    Q_OBJECT
public:
    explicit MetaPanel(QWidget* parent = nullptr);

    // 更新面板显示的元数据内容
    void setTargetFile(const QString& filePath);

private:
    void initReadOnlyArea();
    void initEditableArea();

    QVBoxLayout* m_mainLayout;
    QWidget* m_container;
    QVBoxLayout* m_containerLayout;

    // 只读组件
    QLabel* m_lblName;
    QLabel* m_lblType;
    QLabel* m_lblSize;
    QLabel* m_lblPath;

    // 编辑组件
    QCheckBox* m_chkPinned;
    QWidget* m_ratingWidget;
    QWidget* m_colorWidget;
    QLineEdit* m_tagEdit;
    QPlainTextEdit* m_noteEdit;
};

} // namespace ArcMeta

#endif // META_PANEL_H
