#ifndef BATCH_RENAME_DIALOG_H
#define BATCH_RENAME_DIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QMessageBox>
#include "BatchRenameEngine.h"

namespace ArcMeta {

class BatchRenameDialog : public QDialog {
    Q_OBJECT
public:
    explicit BatchRenameDialog(const QStringList& filePaths, QWidget* parent = nullptr)
        : QDialog(parent), m_filePaths(filePaths) {
        setWindowTitle("批量重命名");
        resize(600, 450);

        auto* layout = new QVBoxLayout(this);

        m_previewList = new QListWidget();
        layout->addWidget(m_previewList);

        m_ruleEdit = new QLineEdit();
        m_ruleEdit->setPlaceholderText("命名规则: ArcMeta_{###} (文本_{序号})");
        m_ruleEdit->setText("File_{###}");
        layout->addWidget(m_ruleEdit);

        auto* btnExecute = new QPushButton("执行重命名");
        layout->addWidget(btnExecute);

        connect(btnExecute, &QPushButton::clicked, this, &BatchRenameDialog::onExecute);
        connect(m_ruleEdit, &QLineEdit::textChanged, this, &BatchRenameDialog::refreshPreview);

        refreshPreview();
    }

private:
    void refreshPreview() {
        m_previewList->clear();
        QString ruleText = m_ruleEdit->text();

        QList<RenameRule> rules;
        if (ruleText.contains("{###}")) {
            QStringList parts = ruleText.split("{###}");
            rules.append({RenameRule::Text, parts[0]});
            rules.append({RenameRule::Number, "1"});
            if (parts.size() > 1) rules.append({RenameRule::Text, parts[1]});
        } else {
            rules.append({RenameRule::Text, ruleText});
        }

        for (int i = 0; i < m_filePaths.size(); ++i) {
            QString oldName = QFileInfo(m_filePaths[i]).fileName();
            QString newName = BatchRenameEngine::generateName(oldName, rules, i);
            m_previewList->addItem(oldName + "  ->  " + newName);
        }
    }

    void onExecute() {
        QString ruleText = m_ruleEdit->text();
        QList<RenameRule> rules;
        if (ruleText.contains("{###}")) {
            QStringList parts = ruleText.split("{###}");
            rules.append({RenameRule::Text, parts[0]});
            rules.append({RenameRule::Number, "1"});
            if (parts.size() > 1) rules.append({RenameRule::Text, parts[1]});
        } else {
            rules.append({RenameRule::Text, ruleText});
        }

        if (BatchRenameEngine::executeRename(m_filePaths, rules)) {
            QMessageBox::information(this, "成功", "批量重命名完成。");
            accept();
        }
    }

    QStringList m_filePaths;
    QListWidget* m_previewList;
    QLineEdit* m_ruleEdit;
};

} // namespace ArcMeta

#endif // BATCH_RENAME_DIALOG_H
