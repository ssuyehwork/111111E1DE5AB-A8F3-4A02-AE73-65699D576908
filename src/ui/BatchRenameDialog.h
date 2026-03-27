#ifndef BATCH_RENAME_DIALOG_H
#define BATCH_RENAME_DIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QLineEdit>
#include "BatchRenameEngine.h"

namespace ArcMeta {

class BatchRenameDialog : public QDialog {
    Q_OBJECT
public:
    explicit BatchRenameDialog(const QStringList& filePaths, QWidget* parent = nullptr)
        : QDialog(parent), m_filePaths(filePaths) {
        setWindowTitle("批量重命名");
        resize(600, 400);

        auto* layout = new QVBoxLayout(this);

        m_previewList = new QListWidget();
        layout->addWidget(m_previewList);

        m_ruleEdit = new QLineEdit();
        m_ruleEdit->setPlaceholderText("命名规则预览: ArcMeta_{###}");
        layout->addWidget(m_ruleEdit);

        auto* btnExecute = new QPushButton("执行重命名");
        layout->addWidget(btnExecute);

        connect(btnExecute, &QPushButton::clicked, this, &BatchRenameDialog::execute);

        refreshPreview();
    }

private:
    void refreshPreview() {
        m_previewList->clear();
        for (int i = 0; i < m_filePaths.size(); ++i) {
            m_previewList->addItem(m_filePaths[i] + " -> NewName_" + QString::number(i));
        }
    }

    void execute() {
        // 执行物理重命名逻辑
        accept();
    }

    QStringList m_filePaths;
    QListWidget* m_previewList;
    QLineEdit* m_ruleEdit;
};

} // namespace ArcMeta

#endif // BATCH_RENAME_DIALOG_H
