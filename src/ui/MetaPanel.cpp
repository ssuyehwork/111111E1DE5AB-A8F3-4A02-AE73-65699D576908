#include "MetaPanel.h"
#include <QScrollArea>
#include <QFileInfo>
#include <QDateTime>
#include <QDir>
#include <QPushButton>
#include <QMap>
#include "../meta/AmMetaJson.h"
#include "../meta/SyncQueue.h"

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

    QFrame* line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    m_containerLayout->addWidget(line);
}

void MetaPanel::initEditableArea() {
    m_chkPinned = new QCheckBox("置顶该项");
    connect(m_chkPinned, &QCheckBox::toggled, this, &MetaPanel::onPinnedToggled);
    m_containerLayout->addWidget(m_chkPinned);

    m_containerLayout->addWidget(new QLabel("星级评分"));
    m_ratingWidget = new QWidget();
    QHBoxLayout* starLayout = new QHBoxLayout(m_ratingWidget);
    starLayout->setContentsMargins(0, 0, 0, 0);
    starLayout->setSpacing(4);
    for (int i = 1; i <= 5; ++i) {
        QPushButton* star = new QPushButton("★");
        star->setFixedSize(20, 20);
        star->setCheckable(true);
        star->setCursor(Qt::PointingHandCursor);
        star->setProperty("rating", i);
        star->setStyleSheet("QPushButton { color: #555; border: none; font-size: 16px; } "
                           "QPushButton:checked { color: #EF9F27; }");
        connect(star, &QPushButton::clicked, [this, i](){ onRatingClicked(i); });
        starLayout->addWidget(star);
    }
    starLayout->addStretch();
    m_containerLayout->addWidget(m_ratingWidget);

    m_containerLayout->addWidget(new QLabel("颜色标记"));
    m_colorWidget = new QWidget();
    QHBoxLayout* colorLayout = new QHBoxLayout(m_colorWidget);
    colorLayout->setContentsMargins(0, 0, 0, 0);
    colorLayout->setSpacing(6);

    QMap<QString, QString> colorMap = {
        {"red", "#E24B4A"}, {"orange", "#EF9F27"}, {"yellow", "#FAC775"},
        {"green", "#639922"}, {"cyan", "#1D9E75"}, {"blue", "#378ADD"},
        {"purple", "#7F77DD"}, {"gray", "#5F5E5A"}
    };

    for (auto it = colorMap.begin(); it != colorMap.end(); ++it) {
        QPushButton* btn = new QPushButton();
        btn->setFixedSize(18, 18);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolTip(it.key());
        btn->setProperty("color", it.key());
        btn->setStyleSheet(QString("background-color: %1; border-radius: 9px; border: none;").arg(it.value()));
        connect(btn, &QPushButton::clicked, [this, k=it.key()](){ onColorClicked(k); });
        colorLayout->addWidget(btn);
    }
    colorLayout->addStretch();
    m_containerLayout->addWidget(m_colorWidget);

    m_containerLayout->addWidget(new QLabel("标签"));
    m_tagEdit = new QLineEdit();
    m_tagEdit->setPlaceholderText("输入标签，逗号分隔...");
    connect(m_tagEdit, &QLineEdit::editingFinished, this, &MetaPanel::onTagsChanged);
    m_containerLayout->addWidget(m_tagEdit);

    m_containerLayout->addWidget(new QLabel("备注"));
    m_noteEdit = new QPlainTextEdit();
    m_noteEdit->setMinimumHeight(100);
    m_noteEdit->setPlaceholderText("在此输入备注内容...");
    connect(m_noteEdit, &QPlainTextEdit::textChanged, this, &MetaPanel::onNoteChanged);
    m_containerLayout->addWidget(m_noteEdit);
}

void MetaPanel::setTargetFile(const QString& filePath) {
    m_currentFilePath = filePath;
    QFileInfo fi(filePath);
    if (!fi.exists()) return;

    m_isLoading = true; // 加载中，禁止自动保存

    m_lblName->setText(fi.fileName());
    m_lblType->setText(fi.isDir() ? "文件夹" : "文件");
    m_lblSize->setText(QString::number(fi.size() / 1024) + " KB");
    m_lblPath->setText(fi.absoluteFilePath());

    // 加载元数据
    AmMeta meta = AmMetaJson::load(fi.absolutePath());
    if (fi.isDir()) {
        m_chkPinned->setChecked(meta.folder.pinned);
        onRatingClicked(meta.folder.rating);
        m_tagEdit->setText(meta.folder.tags.join(", "));
        m_noteEdit->setPlainText(meta.folder.note);
    } else {
        ItemMetadata item = meta.items.value(fi.fileName());
        m_chkPinned->setChecked(item.pinned);
        onRatingClicked(item.rating);
        m_tagEdit->setText(item.tags.join(", "));
        m_noteEdit->setPlainText(item.note);
    }

    m_isLoading = false;
}

void MetaPanel::onRatingClicked(int rating) {
    if (m_isLoading) return;
    for (auto* btn : m_ratingWidget->findChildren<QPushButton*>()) {
        int r = btn->property("rating").toInt();
        btn->setChecked(r <= rating);
    }
    saveCurrentMetadata();
}

void MetaPanel::onColorClicked(const QString& color) {
    if (m_isLoading) return;
    // 简单保存，界面状态可在下次加载时体现
    saveCurrentMetadata();
}

void MetaPanel::onPinnedToggled(bool checked) {
    Q_UNUSED(checked);
    if (m_isLoading) return;
    saveCurrentMetadata();
}

void MetaPanel::onTagsChanged() {
    if (m_isLoading) return;
    saveCurrentMetadata();
}

void MetaPanel::onNoteChanged() {
    if (m_isLoading) return;
    saveCurrentMetadata();
}

void MetaPanel::saveCurrentMetadata() {
    if (m_currentFilePath.isEmpty()) return;

    QFileInfo fi(m_currentFilePath);
    QString dirPath = fi.absolutePath();
    AmMeta meta = AmMetaJson::load(dirPath);

    if (fi.isDir()) {
        meta.folder.pinned = m_chkPinned->isChecked();
        // 获取当前选中的星星数
        int rating = 0;
        for (auto* btn : m_ratingWidget->findChildren<QPushButton*>()) {
            if (btn->isChecked()) rating = qMax(rating, btn->property("rating").toInt());
        }
        meta.folder.rating = rating;
        meta.folder.tags = m_tagEdit->text().split(',', Qt::SkipEmptyParts);
        meta.folder.note = m_noteEdit->toPlainText();
    } else {
        ItemMetadata item = meta.items.value(fi.fileName());
        item.pinned = m_chkPinned->isChecked();
        int rating = 0;
        for (auto* btn : m_ratingWidget->findChildren<QPushButton*>()) {
            if (btn->isChecked()) rating = qMax(rating, btn->property("rating").toInt());
        }
        item.rating = rating;
        item.tags = m_tagEdit->text().split(',', Qt::SkipEmptyParts);
        item.note = m_noteEdit->toPlainText();
        meta.items[fi.fileName()] = item;
    }

    if (AmMetaJson::save(dirPath, meta)) {
        // 触发增量同步到数据库
        SyncQueue::instance().enqueue(dirPath);
    }
}

} // namespace ArcMeta
