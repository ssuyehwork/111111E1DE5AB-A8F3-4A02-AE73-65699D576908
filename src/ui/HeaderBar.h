#ifndef HEADERBAR_H
#define HEADERBAR_H

#include <QWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>

class SearchLineEdit : public QLineEdit {
    Q_OBJECT
public:
    using QLineEdit::QLineEdit;
};

class HeaderBar : public QWidget {
    Q_OBJECT
public:
    explicit HeaderBar(QWidget* parent = nullptr);
    void updatePagination(int current, int total);
    void setFilterActive(bool active);
    void setMetadataActive(bool active);
    void updateToolboxStatus(bool active);
    void focusSearch();

signals:
    void searchChanged(const QString& text);
    void refreshRequested();
    void stayOnTopRequested(bool checked);
    void filterRequested();
    void toggleSidebar();
    void metadataToggled(bool checked);
    void windowMinimize();
    void windowMaximize();
    void windowClose();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    SearchLineEdit* m_searchEdit;
    QLineEdit* m_pageInput;
    QLabel* m_totalPageLabel;
    QPushButton* m_btnFilter;
    QPushButton* m_btnMeta;
    QPushButton* m_btnStayOnTop;
    int m_currentPage = 1;
    int m_totalPages = 1;
};

#endif // HEADERBAR_H
