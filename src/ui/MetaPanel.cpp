#include "MetaPanel.h"
#include <QScrollArea>
#include <QFileInfo>
#include <QDateTime>

namespace ArcMeta {

MetaPanel::MetaPanel(QWidget* parent) : QWidget(parent) {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);

    QScrollArea* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    m_container = new QWidget();
    m_containerLayout = new QVBoxLayout(m_container);
    m_containerLayout->setContentsMargins(12, 12, 12, 12);
    m_containerLayout->setSpacing(15);

    initReadOnlyArea();
    initEditableArea();
    m_containerLayout->addStretch();

    scroll->setWidget(m_container);
    m_mainLayout->addWidget(scroll);
}

void MetaPanel::initReadOnlyArea() {
    auto addInfoLabel = [this](const QString& labelText) {
        QLabel* label = new QLabel(labelText);
        label->setStyleSheet("color: #888; font-size: 11px;");
        m_containerLayout->addWidget(label);
        QLabel* val = new QLabel("-");
        val->setStyleSheet("font-size: 13px; margin-bottom: 5px;");
        val->setWordWrap(true);
        m_containerLayout->addWidget(val);
        return val;
    };

    m_lblName = addInfoLabel("名称");
    m_lblType = addInfoLabel("类型");
    m_lblSize = addInfoLabel("大小");
    m_lblPath = addInfoLabel("路径");

    // 分割线
    QFrame* line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    m_containerLayout->addWidget(line);
}

void MetaPanel::initEditableArea() {
    // 置顶
    m_chkPinned = new QCheckBox("置顶该项");
    m_containerLayout->addWidget(m_chkPinned);

    // 星级（占位）
    m_containerLayout->addWidget(new QLabel("星级评分"));
    m_ratingWidget = new QWidget();
    m_ratingWidget->setFixedHeight(30);
    m_ratingWidget->setStyleSheet("background: #333;");
    m_containerLayout->addWidget(m_ratingWidget);

    // 颜色（占位）
    m_containerLayout->addWidget(new QLabel("颜色标记"));
    m_colorWidget = new QWidget();
    m_colorWidget->setFixedHeight(30);
    m_colorWidget->setStyleSheet("background: #333;");
    m_containerLayout->addWidget(m_colorWidget);

    // 标签
    m_containerLayout->addWidget(new QLabel("标签"));
    m_tagEdit = new QLineEdit();
    m_tagEdit->setPlaceholderText("输入标签，回车添加...");
    m_containerLayout->addWidget(m_tagEdit);

    // 备注
    m_containerLayout->addWidget(new QLabel("备注"));
    m_noteEdit = new QPlainTextEdit();
    m_noteEdit->setMinimumHeight(100);
    m_noteEdit->setPlaceholderText("在此输入备注内容...");
    m_containerLayout->addWidget(m_noteEdit);
}

void MetaPanel::setTargetFile(const QString& filePath) {
    QFileInfo fi(filePath);
    if (!fi.exists()) return;

    m_lblName->setText(fi.fileName());
    m_lblType->setText(fi.isDir() ? "文件夹" : "文件");
    m_lblSize->setText(QString::number(fi.size() / 1024) + " KB");
    m_lblPath->setText(fi.absoluteFilePath());
}

} // namespace ArcMeta
