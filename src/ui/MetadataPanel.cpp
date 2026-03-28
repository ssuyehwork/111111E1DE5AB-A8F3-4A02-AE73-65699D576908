#include "MetadataPanel.h"
#include "TagCapsule.h"
#include "ToolTipOverlay.h"
#include "IconHelper.h"
#include "FlowLayout.h"
#include "../meta/AmMetaJson.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFrame>
#include <QStackedWidget>
#include <QFileInfo>
#include <QDateTime>

MetadataPanel::MetadataPanel(QWidget* parent) : QWidget(parent) {
    setMinimumWidth(230);
    auto* l = new QVBoxLayout(this); l->setContentsMargins(0,0,0,0);
    auto* c = new QFrame(); c->setStyleSheet("background-color: #1e1e1e; border: 1px solid #333;");
    auto* cl = new QVBoxLayout(c);
    
    m_stack = new QStackedWidget();
    m_stack->addWidget(new QLabel("选择文件以查看属性"));
    m_stack->addWidget(new QLabel("不支持多选查看"));
    
    m_metadataDisplayWidget = new QWidget();
    auto* ml = new QVBoxLayout(m_metadataDisplayWidget);
    auto addRow = [&](const QString& lbl, const QString& key) {
        auto* row = new QWidget(); auto* hl = new QHBoxLayout(row);
        hl->addWidget(new QLabel(lbl)); auto* val = new QLabel("-"); m_capsules[key] = val; hl->addWidget(val);
        ml->addWidget(row);
    };
    addRow("创建时间", "created"); addRow("大小", "size"); addRow("星级", "rating");
    
    m_tagContainer = new QWidget(); m_tagFlowLayout = new FlowLayout(m_tagContainer); ml->addWidget(m_tagContainer);
    m_stack->addWidget(m_metadataDisplayWidget);
    cl->addWidget(m_stack);
    
    m_tagEdit = new ClickableLineEdit();
    connect(m_tagEdit, &QLineEdit::returnPressed, this, &MetadataPanel::handleTagInput);
    cl->addWidget(m_tagEdit);
    l->addWidget(c);
}

void MetadataPanel::setItem(const QVariantMap& item) {
    m_currentPath = item.value("content").toString();
    if (m_currentPath.isEmpty()) { clearSelection(); return; }
    m_stack->setCurrentIndex(2);
    QFileInfo info(m_currentPath);
    m_capsules["created"]->setText(info.birthTime().toString("yyyy-MM-dd HH:mm"));
    m_capsules["size"]->setText(QString::number(info.size() / 1024) + " KB");
    QVariantMap meta = AmMetaJson::getItemMeta(m_currentPath);
    int r = meta.value("rating", 0).toInt();
    m_capsules["rating"]->setText(r > 0 ? QString("★").repeated(r) : "未评分");
    refreshTags(meta.value("tags").toStringList().join(", "));
}

void MetadataPanel::refreshTags(const QString& tagsStr) {
    QLayoutItem* child; while ((child = m_tagFlowLayout->takeAt(0))) { if (child->widget()) child->widget()->deleteLater(); delete child; }
    for (const QString& t : tagsStr.split(",", Qt::SkipEmptyParts)) {
        auto* capsule = new TagCapsule(t.trimmed(), m_tagContainer);
        connect(capsule, &TagCapsule::removeRequested, this, &MetadataPanel::removeTag);
        m_tagFlowLayout->addWidget(capsule);
    }
}

void MetadataPanel::handleTagInput() {
    QString t = m_tagEdit->text().trimmed(); if (t.isEmpty() || m_currentPath.isEmpty()) return;
    QVariantMap meta = AmMetaJson::getItemMeta(m_currentPath);
    QStringList tags = meta.value("tags").toStringList(); if (!tags.contains(t)) tags << t;
    AmMetaJson::setItemMeta(m_currentPath, "tags", tags); refreshTags(tags.join(", ")); m_tagEdit->clear();
    emit noteUpdated();
}

void MetadataPanel::removeTag(const QString& t) {
    QVariantMap meta = AmMetaJson::getItemMeta(m_currentPath);
    QStringList tags = meta.value("tags").toStringList(); tags.removeAll(t);
    AmMetaJson::setItemMeta(m_currentPath, "tags", tags); refreshTags(tags.join(", "));
    emit noteUpdated();
}

void MetadataPanel::setMultipleNotes(int) { m_stack->setCurrentIndex(1); }
void MetadataPanel::clearSelection() { m_stack->setCurrentIndex(0); m_currentPath = ""; }
void MetadataPanel::keyPressEvent(QKeyEvent* e) { QWidget::keyPressEvent(e); }
bool MetadataPanel::eventFilter(QObject* w, QEvent* e) { return QWidget::eventFilter(w, e); }
void MetadataPanel::openTagSelector() {}
