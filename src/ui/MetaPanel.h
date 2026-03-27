#ifndef META_PANEL_H
#define META_PANEL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QCheckBox>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QPushButton>

namespace ArcMeta {

class MetaPanel : public QWidget {
    Q_OBJECT
public:
    explicit MetaPanel(QWidget* parent = nullptr);

    // 更新面板显示的元数据内容
    void setTargetFile(const QString& filePath);

signals:
    // 当元数据被修改并保存后触发，通知内容面板刷新
    void metadataUpdated(const QString& filePath);

private slots:
    void onRatingClicked(int rating);
    void onColorClicked(const QString& color);
    void onPinnedToggled(bool checked);
    void onTagsChanged();
    void onNoteChanged();

    // 统一保存逻辑
    void saveCurrentMetadata();

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

    // 当前选中的文件路径
    QString m_currentFilePath;
    bool m_isLoading = false; // 用于避免加载时触发保存
};

} // namespace ArcMeta

#endif // META_PANEL_H
