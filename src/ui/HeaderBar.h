#ifndef HEADERBAR_H
#define HEADERBAR_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QMenu>

class HeaderBar : public QWidget {
    Q_OBJECT
public:
    explicit HeaderBar(QWidget* parent = nullptr);

signals:
    void newNoteRequested();
    void toggleSidebar();
    void globalLockRequested();
    void metadataToggled(bool checked);
    void refreshRequested();
    void filterRequested();
    void stayOnTopRequested(bool checked);
    void windowClose();
    void windowMinimize();
    void windowMaximize();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

public:
    void setFilterActive(bool active);
    void setMetadataActive(bool active);


private:
    QPushButton* m_btnFilter;
    QPushButton* m_btnMeta;
    QPushButton* m_btnStayOnTop;

    QPoint m_dragPos;
};

#endif // HEADERBAR_H
