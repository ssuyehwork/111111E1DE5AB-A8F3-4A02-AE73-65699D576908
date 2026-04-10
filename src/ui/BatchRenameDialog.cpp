#include "BatchRenameDialog.h"
#include "RuleRow.h"
#include "UiHelper.h"
#include "../meta/BatchRenameEngine.h"
#include "../meta/AmMetaJson.h"
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QLabel>
#include <QFile>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QTableWidgetItem>
#include <QRadioButton>

namespace ArcMeta {

BatchRenameDialog::BatchRenameDialog(const std::vector<std::wstring>& originalPaths, QWidget* parent)
    : QDialog(parent), m_originalPaths(originalPaths) {
    setWindowTitle("批量重命名 - ArcMeta");
    resize(700, 600);
    initUi();
    applyTheme();
    onAddRow(); // Initial row
    updatePreview();
}

void BatchRenameDialog::initUi() {
    QVBoxLayout* mainL = new QVBoxLayout(this);
    mainL->setSpacing(15);
    mainL->setContentsMargins(20, 20, 20, 20);

    // --- 1. Target Area ---
    QGroupBox* targetGroup = new QGroupBox("目标操作", this);
    targetGroup->setStyleSheet("QGroupBox { color: #888; font-weight: bold; border: 1px solid #333; margin-top: 10px; padding-top: 10px; } QGroupBox::title { subcontrol-origin: margin; left: 10px; }");
    QVBoxLayout* targetL = new QVBoxLayout(targetGroup);

    QHBoxLayout* rbL = new QHBoxLayout();
    m_rbRename = new QRadioButton("在原目录重命名", this);
    m_rbMove = new QRadioButton("移动到...", this);
    m_rbCopy = new QRadioButton("复制到...", this);
    m_rbRename->setChecked(true);
    rbL->addWidget(m_rbRename);
    rbL->addWidget(m_rbMove);
    rbL->addWidget(m_rbCopy);
    targetL->addLayout(rbL);

    QHBoxLayout* pathL = new QHBoxLayout();
    m_targetPathEdit = new QLineEdit(this);
    m_targetPathEdit->setEnabled(false);
    m_targetPathEdit->setPlaceholderText("选择目标文件夹...");
    m_btnBrowse = new QPushButton("浏览", this);
    m_btnBrowse->setEnabled(false);
    m_btnBrowse->setFixedWidth(80);
    pathL->addWidget(m_targetPathEdit);
    pathL->addWidget(m_btnBrowse);
    targetL->addLayout(pathL);

    mainL->addWidget(targetGroup);

    // --- 2. Rules Area ---
    QLabel* ruleLabel = new QLabel("命名规则构造器", this);
    ruleLabel->setStyleSheet("color: #4ACFEA; font-weight: bold;");
    mainL->addWidget(ruleLabel);

    QScrollArea* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFixedHeight(200);
    scroll->setStyleSheet("QScrollArea { border: 1px solid #333; background: #1E1E1E; }");

    m_rulesContainer = new QWidget(scroll);
    m_rulesLayout = new QVBoxLayout(m_rulesContainer);
    m_rulesLayout->setContentsMargins(10, 10, 10, 10);
    m_rulesLayout->setSpacing(5);
    m_rulesLayout->addStretch();

    scroll->setWidget(m_rulesContainer);
    mainL->addWidget(scroll);

    // --- 3. Preview Area ---
    m_previewTable = new QTableWidget(this);
    m_previewTable->setColumnCount(2);
    m_previewTable->setHorizontalHeaderLabels({"原始名称", "新名称"});
    m_previewTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_previewTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_previewTable->setStyleSheet("QTableWidget { background: #1E1E1E; border: 1px solid #333; gridline-color: #333; } QHeaderView::section { background: #252526; color: #888; border: none; height: 28px; }");
    mainL->addWidget(m_previewTable);

    // --- 4. Bottom Buttons ---
    QHBoxLayout* btnL = new QHBoxLayout();
    m_btnExecute = new QPushButton("执行重命名", this);
    m_btnExecute->setFixedHeight(36);
    m_btnExecute->setStyleSheet("QPushButton { background: #378ADD; color: white; border-radius: 6px; font-weight: bold; } QPushButton:hover { background: #4A9BEF; }");

    m_btnCancel = new QPushButton("取消", this);
    m_btnCancel->setFixedHeight(36);
    m_btnCancel->setStyleSheet("QPushButton { background: #333; color: #EEE; border-radius: 6px; } QPushButton:hover { background: #444; }");

    btnL->addStretch();
    btnL->addWidget(m_btnCancel);
    btnL->addWidget(m_btnExecute);
    mainL->addLayout(btnL);

    // Connections
    connect(m_rbRename, &QRadioButton::toggled, [this](bool checked){
        m_targetPathEdit->setEnabled(!checked);
        m_btnBrowse->setEnabled(!checked);
    });
    connect(m_btnBrowse, &QPushButton::clicked, this, &BatchRenameDialog::onBrowseTarget);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_btnExecute, &QPushButton::clicked, this, &BatchRenameDialog::onExecute);
}

void BatchRenameDialog::applyTheme() {
    setStyleSheet("QDialog { background-color: #1E1E1E; color: #EEE; } QLabel { color: #EEE; } QRadioButton { color: #EEE; } QLineEdit { background: #1E1E1E; border: 1px solid #444; border-radius: 4px; padding: 2px 5px; color: #EEE; }");
}

void BatchRenameDialog::onAddRow() {
    RuleRow* row = new RuleRow(m_rulesContainer);
    // Insert before the stretch
    m_rulesLayout->insertWidget(m_rulesLayout->count() - 1, row);
    m_ruleRows.append(row);

    connect(row, &RuleRow::changed, this, &BatchRenameDialog::updatePreview);
    connect(row, &RuleRow::addRequested, this, &BatchRenameDialog::onAddRow);
    connect(row, &RuleRow::removeRequested, [this, row]() {
        if (m_ruleRows.size() > 1) {
            m_ruleRows.removeOne(row);
            row->deleteLater();
            updatePreview();
        }
    });
    updatePreview();
}

void BatchRenameDialog::updatePreview() {
    std::vector<RenameRule> rules;
    for (auto* row : m_ruleRows) rules.push_back(row->getRule());

    auto newNames = BatchRenameEngine::instance().preview(m_originalPaths, rules);

    int previewCount = qMin((int)m_originalPaths.size(), 100);
    m_previewTable->setRowCount(previewCount);

    for (int i = 0; i < previewCount; ++i) {
        QFileInfo oldInfo(QString::fromStdWString(m_originalPaths[i]));
        m_previewTable->setItem(i, 0, new QTableWidgetItem(oldInfo.fileName()));

        auto* newItem = new QTableWidgetItem(QString::fromStdWString(newNames[i]));
        newItem->setForeground(QColor("#2ecc71"));
        m_previewTable->setItem(i, 1, newItem);
    }
}

void BatchRenameDialog::onBrowseTarget() {
    QString dir = QFileDialog::getExistingDirectory(this, "选择目标文件夹");
    if (!dir.isEmpty()) {
        m_targetPathEdit->setText(dir);
    }
}

void BatchRenameDialog::onExecute() {
    std::vector<RenameRule> rules;
    for (auto* row : m_ruleRows) rules.push_back(row->getRule());

    auto newNames = BatchRenameEngine::instance().preview(m_originalPaths, rules);
    QString targetDir = m_targetPathEdit->text();

    if ((m_rbMove->isChecked() || m_rbCopy->isChecked()) && targetDir.isEmpty()) {
        QMessageBox::warning(this, "错误", "请先选择目标文件夹");
        return;
    }

    int successCount = 0;
    for (size_t i = 0; i < m_originalPaths.size(); ++i) {
        QString oldPath = QString::fromStdWString(m_originalPaths[i]);
        QFileInfo oldInfo(oldPath);
        QString finalTargetDir = m_rbRename->isChecked() ? oldInfo.absolutePath() : targetDir;
        QString newPath = QDir(finalTargetDir).filePath(QString::fromStdWString(newNames[i]));

        bool ok = false;
        if (m_rbCopy->isChecked()) {
            ok = QFile::copy(oldPath, newPath);
        } else if (m_rbMove->isChecked()) {
            if (QFile::copy(oldPath, newPath)) {
                ok = QFile::remove(oldPath);
            }
        } else {
            ok = QFile::rename(oldPath, newPath);
        }

        if (ok) {
            successCount++;
            // 迁移元数据逻辑 (物理重命名后的关键同步)
            if (!m_rbCopy->isChecked()) {
                AmMetaJson::renameItem(oldInfo.absolutePath(), oldInfo.fileName(), QString::fromStdWString(newNames[i]));
            }
        }
    }

    QMessageBox::information(this, "操作完成", QString("成功处理 %1 个文件").arg(successCount));
    accept();
}

} // namespace ArcMeta
