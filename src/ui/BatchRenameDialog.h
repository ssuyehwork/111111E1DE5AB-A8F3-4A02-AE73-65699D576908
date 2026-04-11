#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QRadioButton>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QGroupBox>
#include <QList>
#include <vector>
#include <string>

namespace ArcMeta {

class RuleRow;

/**
 * @brief 批量重命名高级对话框
 */
class BatchRenameDialog : public QDialog {
    Q_OBJECT
public:
    explicit BatchRenameDialog(const std::vector<std::wstring>& originalPaths, QWidget* parent = nullptr);
    ~BatchRenameDialog() override = default;

private slots:
    void onAddRow();
    void updatePreview();
    void onExecute();
    void onBrowseTarget();

private:
    void initUi();
    void applyTheme();

    std::vector<std::wstring> m_originalPaths;
    
    QRadioButton* m_rbRename = nullptr;
    QRadioButton* m_rbMove = nullptr;
    QRadioButton* m_rbCopy = nullptr;
    
    QLineEdit* m_targetPathEdit = nullptr;
    QPushButton* m_btnBrowse = nullptr;
    
    QWidget* m_rulesContainer = nullptr;
    QVBoxLayout* m_rulesLayout = nullptr;
    QList<RuleRow*> m_ruleRows;
    
    QTableWidget* m_previewTable = nullptr;
    
    QPushButton* m_btnExecute = nullptr;
    QPushButton* m_btnCancel = nullptr;
};

} // namespace ArcMeta
