#include "Editor.h"
#include "StringUtils.h"
#include <QMimeData>
#include <QFileInfo>
#include <utility>
#include <QUrl>
#include <QTextList>

MarkdownHighlighter::MarkdownHighlighter(QTextDocument* parent) : QSyntaxHighlighter(parent) {
    HighlightingRule rule;

    // 标题 (Headers) - 蓝色
    QTextCharFormat headerFormat;
    headerFormat.setForeground(QColor("#569CD6"));
    headerFormat.setFontWeight(QFont::Bold);
    rule.pattern = QRegularExpression("^#{1,6}\\s.*");
    rule.format = headerFormat;
    m_highlightingRules.append(rule);

    // 粗体 (**bold**) - 红色
    QTextCharFormat boldFormat;
    boldFormat.setFontWeight(QFont::Bold);
    boldFormat.setForeground(QColor("#E06C75"));
    rule.pattern = QRegularExpression("\\*\\*.*?\\*\\*");
    rule.format = boldFormat;
    m_highlightingRules.append(rule);

    // 待办事项 ([ ] [x]) - 黄色/绿色
    QTextCharFormat uncheckedFormat;
    uncheckedFormat.setForeground(QColor("#E5C07B"));
    rule.pattern = QRegularExpression("-\\s\\[\\s\\]");
    rule.format = uncheckedFormat;
    m_highlightingRules.append(rule);

    QTextCharFormat checkedFormat;
    checkedFormat.setForeground(QColor("#6A9955"));
    rule.pattern = QRegularExpression("-\\s\\[x\\]");
    rule.format = checkedFormat;
    m_highlightingRules.append(rule);

    // 代码 (Code) - 绿色
    QTextCharFormat codeFormat;
    codeFormat.setForeground(QColor("#98C379"));
    codeFormat.setFontFamilies({"Consolas", "Monaco", "monospace"});
    rule.pattern = QRegularExpression("`[^`]+`|