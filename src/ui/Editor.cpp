#include "Editor.h"
#include "StringUtils.h"
#include <QMimeData>
#include <QFileInfo>
#include <utility>
#include <QUrl>
#include <QTextList>
#include <QVBoxLayout>
#include <QStackedWidget>

MarkdownHighlighter::MarkdownHighlighter(QTextDocument* parent) : QSyntaxHighlighter(parent) {
    HighlightingRule rule;
    QTextCharFormat headerFormat; headerFormat.setForeground(QColor("#569CD6")); headerFormat.setFontWeight(QFont::Bold);
    rule.pattern = QRegularExpression("^#{1,6}\\s.*"); rule.format = headerFormat; m_highlightingRules.append(rule);
    QTextCharFormat boldFormat; boldFormat.setFontWeight(QFont::Bold); boldFormat.setForeground(QColor("#E06C75"));
    rule.pattern = QRegularExpression("\\*\\*.*?\\*\\*"); rule.format = boldFormat; m_highlightingRules.append(rule);
}
void MarkdownHighlighter::highlightBlock(const QString& text) {
    for (const HighlightingRule& rule : m_highlightingRules) {
        QRegularExpressionMatchIterator i = rule.pattern.globalMatch(text);
        while (i.hasNext()) { QRegularExpressionMatch m = i.next(); setFormat(m.capturedStart(), m.capturedLength(), rule.format); }
    }
}

InternalEditor::InternalEditor(QWidget* parent) : QTextEdit(parent) {
    setStyleSheet("background: #1E1E1E; color: #D4D4D4; font-family: 'Consolas'; font-size: 13pt; border: none;");
}
void InternalEditor::insertTodo() {}
void InternalEditor::highlightSelection(const QColor&) {}
void InternalEditor::insertFromMimeData(const QMimeData* s) { QTextEdit::insertFromMimeData(s); }
void InternalEditor::keyPressEvent(QKeyEvent* e) { QTextEdit::keyPressEvent(e); }

Editor::Editor(QWidget* parent) : QWidget(parent) {
    auto* l = new QVBoxLayout(this); l->setContentsMargins(0, 0, 0, 0);
    m_stack = new QStackedWidget(this);
    m_edit = new InternalEditor(this);
    m_preview = new QTextEdit(this); m_preview->setReadOnly(true);
    m_stack->addWidget(m_edit); m_stack->addWidget(m_preview);
    l->addWidget(m_stack);
}
void Editor::setItem(const QVariantMap& item, bool isPreview) {
    m_currentItem = item;
    QString title = item.value("title").toString();
    QString content = item.value("content").toString();
    if (isPreview) {
        m_preview->setHtml(StringUtils::generatePreviewHtml(title, content, item.value("item_type").toString(), item.value("data_blob").toByteArray()));
        m_stack->setCurrentWidget(m_preview);
    } else {
        m_edit->setPlainText(content);
        m_stack->setCurrentWidget(m_edit);
    }
}
void Editor::setPlainText(const QString& t) { m_edit->setPlainText(t); m_preview->setPlainText(t); }
QString Editor::toPlainText() const { return m_edit->toPlainText(); }
QString Editor::toHtml() const { return m_edit->toHtml(); }
bool Editor::isRich() const { return false; }
QString Editor::getOptimizedContent() const { return toPlainText(); }
void Editor::setPlaceholderText(const QString& t) { m_edit->setPlaceholderText(t); }
void Editor::clearFormatting() {}
void Editor::toggleList(bool) {}
bool Editor::findText(const QString&, bool) { return false; }
void Editor::togglePreview(bool p) { m_stack->setCurrentIndex(p ? 1 : 0); }
void Editor::setReadOnly(bool r) { m_edit->setReadOnly(r); }
bool Editor::eventFilter(QObject* w, QEvent* e) { return QWidget::eventFilter(w, e); }
