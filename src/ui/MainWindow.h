#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeView>
#include <QListView>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QSplitter>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QTimer>
#include <QFileInfo>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>
#include <QStackedWidget>
#include <QSlider>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QWheelEvent>
#include <QList>
#include <QVariantMap>
#include "HeaderBar.h"
#include "MetadataPanel.h"
#include "FilterPanel.h"
#include "FileItemDelegate.h"
#include "IconHelper.h"
#include "../core/DatabaseManager.h"
#include "../meta/AmMetaJson.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#endif

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

    // [RULE] MSVC 编译环境下，必须在头文件中内联实现以防链接错误
    inline void loadDirectory(const QString& path) {
        if (path.isEmpty()) return;
        m_fileModel->clear();
        QDir dir(path);
        if (!dir.exists()) return;

        m_itemCountLabel->setText("正在加载...");
        QFileInfoList list = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::DirsFirst | QDir::Name);
        QVariantMap folderMeta = AmMetaJson::read(path);
        QVariantMap itemsMeta = folderMeta.value("items").toMap();

        for (const QFileInfo& info : list) {
            if (info.fileName() == ".am_meta.json") continue;
            auto* item = new QStandardItem(info.fileName());
            item->setData(info.absoluteFilePath(), (int)FileItemDelegate::PathRole);
            item->setData(info.isDir() ? "folder" : "file", (int)FileItemDelegate::TypeRole);
            item->setIcon(IconHelper::getIcon(info.isDir() ? "folder" : "file", info.isDir() ? "#F2B705" : "#CCC"));
            
            QVariantMap meta = itemsMeta.value(info.fileName()).toMap();
            item->setData(meta.value("rating", 0).toInt(), (int)FileItemDelegate::RatingRole);
            item->setData(meta.value("color", "").toString(), (int)FileItemDelegate::ColorRole);
            item->setData(meta.value("pinned", false).toBool(), (int)FileItemDelegate::IsPinnedRole);
            
            m_fileModel->appendRow(item);
        }
        m_itemCountLabel->setText(QString("%1 个项目").arg(m_fileModel->rowCount()));
    }

    inline bool setFileLock(const QString& path, bool lock) {
        Q_UNUSED(path); Q_UNUSED(lock);
        return false;
    }

    inline void wheelEvent(QWheelEvent* event) override {
        if (event->modifiers() & Qt::ControlModifier) {
            int delta = event->angleDelta().y();
            int newValue = m_zoomSlider->value() + (delta > 0 ? 10 : -10);
            m_zoomSlider->setValue(qBound(m_zoomSlider->minimum(), newValue, m_zoomSlider->maximum()));
            event->accept();
            return;
        }
        QMainWindow::wheelEvent(event);
    }

signals:
    void toolboxRequested();
    void globalLockRequested();

public:
    void updateToolboxStatus(bool active);

private slots:
    void onSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
    void showContextMenu(const QPoint& pos);
    void saveLayout();
    void restoreLayout();
    void updateShortcuts();
    void refreshData();
    void loadDrives();
    void onItemExpanded(QTreeWidgetItem* item);
    void doDeleteSelected(bool physical = false);
    void doTogglePin();
    void doSetRating(int rating);
    void doCopyTags();
    void doPasteTags();
    void doRepeatAction();
    bool verifyExportPermission();

protected:
#ifdef Q_OS_WIN
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#endif
    bool event(QEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void initUI();
    void setupShortcuts();
    void updateFocusLines();
    
    QTreeWidget* m_placesTree;
    QWidget* m_categoryContainer;
    QWidget* m_navContainer;
    QWidget* m_favoritesContainer;
    QWidget* m_contentContainer;
    QWidget* m_listFocusLine;
    QWidget* m_sidebarFocusLine;
    QTreeWidget* m_navTree;
    HeaderBar* m_header;
    MetadataPanel* m_metaPanel;
    FilterPanel* m_filterPanel;
    QWidget* m_filterWrapper;
    QListView* m_fileView;
    QStandardItemModel* m_fileModel;
    QWidget* m_contentStatusBar;
    QSlider* m_zoomSlider;
    QPushButton* m_btnGridView;
    QPushButton* m_btnListView;
    QCheckBox* m_checkThumbnailOnly;
    QCheckBox* m_checkTile;
    QLabel* m_itemCountLabel;
    QPushButton* m_editBtn;

    QString m_currentKeyword;
    QString m_currentFilterType = "all";
    QVariant m_currentFilterValue = -1;
    int m_currentPage = 1;
    int m_pageSize = DatabaseManager::DEFAULT_PAGE_SIZE;
    QTimer* m_searchTimer;
    QTimer* m_refreshTimer;
};

#endif // MAINWINDOW_H
