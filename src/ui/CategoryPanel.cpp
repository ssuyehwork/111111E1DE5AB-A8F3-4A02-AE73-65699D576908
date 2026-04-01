#include "CategoryPanel.h"
#include "CategoryModel.h"
#include "CategoryDelegate.h"
#include "DropTreeView.h"
#include "UiHelper.h"
#include "ToolTipOverlay.h"
#include "FramelessDialog.h"
#include "../db/CategoryRepo.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QHeaderView>
#include <QScrollBar>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QColorDialog>
#include <QRandomGenerator>

namespace ArcMeta {

CategoryPanel::CategoryPanel(QWidget* parent)
    : QFrame(parent) {
    setObjectName("SidebarContainer");
    setAttribute(Qt::WA_StyledBackground, true);
    setMinimumWidth(230);
    setStyleSheet("color: #EEEEEE;");
    
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    initUi();
    setupContextMenu();
}

void CategoryPanel::setupContextMenu() {
    m_partitionTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_partitionTree, &QWidget::customContextMenuRequested, [this](const QPoint& pos) {
        QModelIndex index = m_partitionTree->indexAt(pos);
        auto selected = m_partitionTree->selectionModel()->selectedIndexes();
        
        QMenu menu(this);
        menu.setStyleSheet("QMenu { background-color: #2D2D2D; color: #EEE; border: 1px solid #444; padding: 4px; border-radius: 8px; } "
                           "QMenu::item { padding: 6px 10px 6px 10px; border-radius: 4px; } "
                           "QMenu::icon { margin-left: 6px; } "
                           "QMenu::item:selected { background-color: #3E3E42; color: white; }");

        QString idxName = index.data(CategoryModel::NameRole).toString();
        int catId = index.data(CategoryModel::IdRole).toInt();

        // 1. 如果没有选中项，或者选中了“我的分类”根节点
        if (!index.isValid() || idxName == "我的分类") {
            menu.addAction(UiHelper::getIcon("add", QColor("#3498db"), 18), "新建分类", this, &CategoryPanel::onCreateCategory);
        } 
        // 2. 如果是具体的分类项
        else if (catId > 0) {
            // --- 笔记/数据相关 ---
            menu.addAction(UiHelper::getIcon("add", QColor("#2ECC71"), 18), "新建数据", [this]() {
                // 物理还原：调用 MainWindow 的内容创建逻辑
                QWidget* mw = window();
                if (mw) QMetaObject::invokeMethod(mw, "createNewItem", Q_ARG(QString, "md"));
            });

            menu.addAction(UiHelper::getIcon("branch", QColor("#3498db"), 18), "归类到此分类", [idxName]() {
                ToolTipOverlay::instance()->showText(QCursor::pos(), 
                    QString("<b style='color: #3498db;'>[OK] 已指定插件归类到: %1</b>").arg(idxName));
            });

            menu.addSeparator();

            // --- 颜色管理 ---
            auto* colorMenu = menu.addMenu(UiHelper::getIcon("palette", QColor("#aaaaaa"), 18), "设置颜色");
            colorMenu->setStyleSheet(menu.styleSheet());
            
            QStringList palette = {
                "#FF6B6B", "#4ECDC4", "#45B7D1", "#96CEB4", "#FFEEAD",
                "#D4A5A5", "#9B59B6", "#3498DB", "#E67E22", "#2ECC71",
                "#E74C3C", "#F1C40F", "#1ABC9C", "#34495E", "#95A5A6"
            };

            for (const QString& col : palette) {
                QPixmap pix(16, 16);
                pix.fill(QColor(col));
                colorMenu->addAction(QIcon(pix), col, [this, catId, col]() {
                    Category cat = CategoryRepo::getById(catId);
                    if (cat.id > 0) {
                        cat.color = col.toStdWString();
                        CategoryRepo::update(cat);
                        m_partitionModel->refresh();
                    }
                });
            }

            menu.addAction(UiHelper::getIcon("refresh", QColor("#aaaaaa"), 18), "随机颜色", [this, catId, palette]() {
                QString chosenColor = palette.at(QRandomGenerator::global()->bounded(palette.size()));
                Category cat = CategoryRepo::getById(catId);
                if (cat.id > 0) {
                    cat.color = chosenColor.toStdWString();
                    CategoryRepo::update(cat);
                    m_partitionModel->refresh();
                }
            });

            menu.addAction(UiHelper::getIcon("tag", QColor("#FFAB91"), 18), "设置预设标签", [this, catId]() {
                Category cat = CategoryRepo::getById(catId);
                if (cat.id > 0) {
                    QString tagsStr;
                    for(const auto& t : cat.presetTags) { if(!tagsStr.isEmpty()) tagsStr += ","; tagsStr += QString::fromStdWString(t); }
                    
                    FramelessInputDialog dlg("设置预设标签", "标签 (逗号分隔):", tagsStr, this);
                    if (dlg.exec() == QDialog::Accepted) {
                        QString text = dlg.textValue();
                        cat.presetTags.clear();
                        QStringList list = text.split(",", Qt::SkipEmptyParts);
                        for(const auto& t : list) cat.presetTags.push_back(t.trimmed().toStdWString());
                        CategoryRepo::update(cat);
                    }
                }
            });

            menu.addSeparator();

            // --- 结构管理 ---
            menu.addAction(UiHelper::getIcon("add", QColor("#aaaaaa"), 18), "新建分类", this, &CategoryPanel::onCreateCategory);
            menu.addAction(UiHelper::getIcon("add", QColor("#3498db"), 18), "新建子分类", [this, catId]() {
                FramelessInputDialog dlg("新建子分类", "区名称:", "", this);
                if (dlg.exec() == QDialog::Accepted) {
                    QString text = dlg.textValue();
                    if (text.isEmpty()) return;
                    Category cat;
                    cat.name = text.toStdWString();
                    cat.parentId = catId;
                    cat.color = L"#3498db";
                    CategoryRepo::add(cat);
                    m_partitionModel->refresh();
                }
            });

            menu.addSeparator();

            if (selected.size() == 1) {
                bool isPinned = index.data(CategoryModel::PinnedRole).toBool();
                menu.addAction(UiHelper::getIcon(isPinned ? "pin_vertical" : "pin_tilted", isPinned ? QColor("#FF551C") : QColor("#aaaaaa"), 18), 
                               isPinned ? "取消置顶" : "置顶分类", [this, catId, isPinned]() {
                    Category cat = CategoryRepo::getById(catId);
                    if (cat.id > 0) {
                        cat.pinned = !isPinned;
                        CategoryRepo::update(cat);
                        m_partitionModel->refresh();
                    }
                });
                
                menu.addAction(UiHelper::getIcon("edit", QColor("#aaaaaa"), 18), "重命名分类", [this, index]() {
                    m_partitionTree->edit(index);
                });
            }

            QString deleteText = selected.size() > 1 ? QString("删除选中的 %1 个分类").arg(selected.size()) : "删除分类";
            menu.addAction(UiHelper::getIcon("trash", QColor("#e74c3c"), 18), deleteText, [this, selected]() {
                if (FramelessMessageBox::question(this, "确认删除", "确定要删除选中的分类吗？\n(注意：物理文件不会删除，但分类关系将被抹除)")) {
                    for (const auto& idx : selected) {
                        int id = idx.data(CategoryModel::IdRole).toInt();
                        if (id > 0) CategoryRepo::remove(id);
                    }
                    m_partitionModel->refresh();
                }
            });

            menu.addSeparator();

            // --- 排列与保护 (框架) ---
            auto* sortMenu = menu.addMenu(UiHelper::getIcon("list_ol", QColor("#aaaaaa"), 18), "排列");
            sortMenu->setStyleSheet(menu.styleSheet());
            sortMenu->addAction("标题(当前层级) (A→Z)");
            sortMenu->addAction("标题(当前层级) (Z→A)");
            sortMenu->addAction("标题(全部) (A→Z)");
            sortMenu->addAction("标题(全部) (Z→A)");

            menu.addSeparator();
            auto* pwdMenu = menu.addMenu(UiHelper::getIcon("lock", QColor("#aaaaaa"), 18), "密码保护");
            pwdMenu->setStyleSheet(menu.styleSheet());
            pwdMenu->addAction("设置");
            pwdMenu->addAction("修改");
            pwdMenu->addAction("移除");
            pwdMenu->addAction("立即锁定");
        }
        
        if (!menu.isEmpty()) {
            menu.exec(m_partitionTree->viewport()->mapToGlobal(pos));
        }
    });
}

void CategoryPanel::onCreateCategory() {
    FramelessInputDialog dlg("新建分类", "名称:", "", this);
    if (dlg.exec() == QDialog::Accepted) {
        QString text = dlg.textValue();
        if (text.isEmpty()) return;
        Category cat;
        cat.name = text.toStdWString();
        cat.parentId = 0;
        cat.color = L"#3498db";
        CategoryRepo::add(cat);
        m_partitionModel->refresh();
    }
}

void CategoryPanel::onRenameCategory() {
    QModelIndex index = m_partitionTree->currentIndex();
    if (index.isValid()) m_partitionTree->edit(index);
}

void CategoryPanel::onDeleteCategory() {
    QModelIndex index = m_partitionTree->currentIndex();
    if (index.isValid()) {
        int id = index.data(CategoryModel::IdRole).toInt();
        if (id > 0) {
            ArcMeta::CategoryRepo::remove(id);
            m_partitionModel->refresh();
        }
    }
}

void CategoryPanel::initUi() {
    // 物理还原：根据用户要求，分类容器保持左侧 10 像素边距，上方同样保持 10 像素边距
    m_mainLayout->setContentsMargins(10, 10, 10, 10);
    m_mainLayout->setSpacing(0);

    QString treeStyle = R"(
        QTreeView { background-color: transparent; border: none; color: #CCC; outline: none; }
        QTreeView::branch { width: 0px; }
        QTreeView::branch:has-children:closed { image: url(:/icons/arrow_right.svg); width: 12px; }
        QTreeView::branch:has-children:open   { image: url(:/icons/arrow_down.svg); width: 12px; }
        QTreeView::item { height: 22px; padding-left: 0px; margin-left: 0px; }
    )";

    // 系统树
    m_systemTree = new DropTreeView(this);
    m_systemTree->setStyleSheet(treeStyle); 
    m_systemTree->setItemDelegate(new CategoryDelegate(this));
    m_systemModel = new CategoryModel(CategoryModel::System, this);
    m_systemTree->setModel(m_systemModel);
    m_systemTree->setHeaderHidden(true);
    m_systemTree->setRootIsDecorated(false);
    m_systemTree->setIndentation(0);
    m_systemTree->setFixedHeight(176); // 8 items * 22px
    m_systemTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_systemTree->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    // 分区树
    m_partitionTree = new DropTreeView(this);
    m_partitionTree->setStyleSheet(treeStyle);
    m_partitionTree->setItemDelegate(new CategoryDelegate(this));
    m_partitionModel = new CategoryModel(CategoryModel::User, this);
    m_partitionTree->setModel(m_partitionModel);
    m_partitionTree->setHeaderHidden(true);
    m_partitionTree->setRootIsDecorated(true);
    m_partitionTree->setIndentation(0);
    m_partitionTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_partitionTree->setDragEnabled(true);
    m_partitionTree->setAcceptDrops(true);
    m_partitionTree->setDropIndicatorShown(true);
    m_partitionTree->setDragDropMode(QAbstractItemView::InternalMove);
    m_partitionTree->setDefaultDropAction(Qt::MoveAction);
    m_partitionTree->expandAll();
    m_partitionTree->setSelectionMode(QAbstractItemView::ExtendedSelection);

    connect(m_systemTree, &QTreeView::clicked, [this](const QModelIndex& index) {
        emit categorySelected(index.data(CategoryModel::NameRole).toString());
    });
    connect(m_partitionTree, &QTreeView::clicked, [this](const QModelIndex& index) {
        emit categorySelected(index.data(CategoryModel::NameRole).toString());
    });

    connect(m_partitionTree, &DropTreeView::pathsDropped, [this](const QStringList& paths, const QModelIndex& index) {
        if (!index.isValid()) return;
        
        int categoryId = index.data(CategoryModel::IdRole).toInt();
        QString categoryName = index.data(CategoryModel::NameRole).toString();
        
        // 核心红线：仅对具体分类执行“建立引用收藏”逻辑
        if (categoryId > 0) {
            int count = 0;
            for (const QString& path : paths) {
                if (CategoryRepo::addItemToCategory(categoryId, path.toStdWString())) {
                    count++;
                }
            }
            // 物理反馈：提示收藏成功
            ToolTipOverlay::instance()->showText(QCursor::pos(), 
                QString("<b style='color:#2ecc71;'>已收藏 %1 个项目到 [%2]</b>").arg(count).arg(categoryName), 1200);
        }
    });
    
    m_mainLayout->addWidget(m_systemTree);
    m_mainLayout->addWidget(m_partitionTree, 1);
}

bool CategoryPanel::eventFilter(QObject* obj, QEvent* event) {
    return QFrame::eventFilter(obj, event);
}

} // namespace ArcMeta
