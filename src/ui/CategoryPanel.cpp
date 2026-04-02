#include "CategoryPanel.h"
#include "CategoryModel.h"
#include "CategoryDelegate.h"
#include "DropTreeView.h"
#include "UiHelper.h"
#include "ToolTipOverlay.h"
#include "FramelessDialog.h"
#include <QDir>
#include "../db/CategoryRepo.h"
#include "../meta/MetadataManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QHeaderView>
#include <QScrollBar>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QRandomGenerator>
#include <QSet>
#include <QSettings>

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
    m_categoryTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_categoryTree, &QWidget::customContextMenuRequested, [this](const QPoint& pos) {
        QModelIndex index = m_categoryTree->indexAt(pos);
        
        QMenu menu(this);
        // [PHYSICAL RESTORATION] 8px radius for context menu
        menu.setStyleSheet("QMenu { background-color: #2D2D2D; color: #EEE; border: 1px solid #444; padding: 4px; border-radius: 8px; } "
                           "QMenu::item { padding: 6px 25px 6px 10px; border-radius: 4px; } "
                           "QMenu::icon { margin-left: 6px; } "
                           "QMenu::item:selected { background-color: #3E3E42; color: white; } "
                           "QMenu::separator { height: 1px; background: #444; margin: 4px 8px; }");

        // 基于规范逻辑：如果没有选中项，或者选中了“我的分类”根节点
        if (!index.isValid() || index.data(CategoryModel::NameRole).toString() == "我的分类") {
            menu.addAction(UiHelper::getIcon("add", QColor("#aaaaaa"), 18), "新建分类", this, &CategoryPanel::onCreateCategory);
            
            auto* sortMenu = menu.addMenu(UiHelper::getIcon("list_ul", QColor("#aaaaaa"), 18), "排列");
            sortMenu->setStyleSheet(menu.styleSheet());
            sortMenu->addAction("按名称");
            sortMenu->addAction("按时间");
        } else {
            // 具体分类项的右键菜单
            QString type = index.data(CategoryModel::TypeRole).toString();
            if (type == "category") {
                menu.addAction(UiHelper::getIcon("add", QColor("#3498db"), 18), "新建数据", this, &CategoryPanel::onAddData);
                menu.addAction(UiHelper::getIcon("branch", QColor("#3498db"), 18), "归类到此分类", this, &CategoryPanel::onClassifyToCategory);
                
                menu.addSeparator();
                
                menu.addAction(UiHelper::getIcon("palette", QColor("#f39c12"), 18), "设置颜色", this, &CategoryPanel::onSetColor);
                menu.addAction(UiHelper::getIcon("random_color", QColor("#e91e63"), 18), "随机颜色", this, &CategoryPanel::onRandomColor);
                menu.addAction(UiHelper::getIcon("tag_filled", QColor("#9b59b6"), 18), "设置预设标签", this, &CategoryPanel::onSetPresetTags);

                menu.addSeparator();

                menu.addAction(UiHelper::getIcon("add", QColor("#aaaaaa"), 18), "新建分类", this, &CategoryPanel::onCreateCategory);
                menu.addAction(UiHelper::getIcon("add", QColor("#aaaaaa"), 18), "新建子分类", this, &CategoryPanel::onCreateSubCategory);

                menu.addSeparator();

                bool isPinned = index.data(CategoryModel::PinnedRole).toBool();
                menu.addAction(UiHelper::getIcon("pin_vertical", isPinned ? QColor("#FF551C") : QColor("#aaaaaa"), 18), 
                               isPinned ? "取消置顶" : "置顶分类", this, &CategoryPanel::onTogglePin);
                               
                menu.addAction(UiHelper::getIcon("edit", QColor("#aaaaaa"), 18), "重命名分类", this, &CategoryPanel::onRenameCategory);
                menu.addAction(UiHelper::getIcon("trash", QColor("#e74c3c"), 18), "删除分类", this, &CategoryPanel::onDeleteCategory);

                menu.addSeparator();

                auto* sortMenu = menu.addMenu(UiHelper::getIcon("list_ul", QColor("#aaaaaa"), 18), "排列");
                sortMenu->setStyleSheet(menu.styleSheet());
                sortMenu->addAction("按名称");
                sortMenu->addAction("按时间");

                auto* pwdMenu = menu.addMenu(UiHelper::getIcon("lock", QColor("#aaaaaa"), 18), "密码保护");
                pwdMenu->setStyleSheet(menu.styleSheet());
                pwdMenu->addAction("设置密码", this, &CategoryPanel::onSetPassword);
                pwdMenu->addAction("清除密码", this, &CategoryPanel::onClearPassword);
            }
        }
        
        if (!menu.isEmpty()) {
            menu.exec(m_categoryTree->viewport()->mapToGlobal(pos));
        }
    });
}

/**
 * @brief 递归保存 QTreeView 的展开状态
 */
static void saveExpandedState(QTreeView* tree, const QModelIndex& parent, QSet<int>& expandedIds, QStringList& expandedNames) {
    for (int i = 0; i < tree->model()->rowCount(parent); ++i) {
        QModelIndex idx = tree->model()->index(i, 0, parent);
        if (tree->isExpanded(idx)) {
            int id = idx.data(CategoryModel::IdRole).toInt();
            if (id > 0) expandedIds.insert(id);
            else expandedNames << idx.data(CategoryModel::NameRole).toString();
            saveExpandedState(tree, idx, expandedIds, expandedNames);
        }
    }
}

/**
 * @brief 递归恢复 QTreeView 的展开状态
 */
static void restoreExpandedState(QTreeView* tree, const QModelIndex& parent, const QSet<int>& expandedIds, const QStringList& expandedNames) {
    for (int i = 0; i < tree->model()->rowCount(parent); ++i) {
        QModelIndex idx = tree->model()->index(i, 0, parent);
        int id = idx.data(CategoryModel::IdRole).toInt();
        QString name = idx.data(CategoryModel::NameRole).toString();
        
        bool shouldExpand = false;
        if (expandedNames.contains(name) || (id > 0 && expandedIds.contains(id)) || name == "我的分类") {
            shouldExpand = true;
        }

        if (shouldExpand) {
            tree->setExpanded(idx, true);
            restoreExpandedState(tree, idx, expandedIds, expandedNames);
        }
    }
}

void CategoryPanel::onCreateCategory() {
    FramelessInputDialog dlg("新建分类", "请输入分类名称:", "", this);
    if (dlg.exec() == QDialog::Accepted) {
        QString text = dlg.text();
        if (!text.isEmpty()) {
            Category cat;
            cat.name = text.toStdWString();
            cat.parentId = 0;
            cat.color = L"#3498db";
            
            QSet<int> expandedIds;
            QStringList expandedNames;
            saveExpandedState(m_categoryTree, QModelIndex(), expandedIds, expandedNames);

            CategoryRepo::add(cat);
            m_categoryModel->refresh();

            restoreExpandedState(m_categoryTree, QModelIndex(), expandedIds, expandedNames);
        }
    }
}

void CategoryPanel::onCreateSubCategory() {
    QModelIndex index = m_categoryTree->currentIndex();
    if (!index.isValid()) return;
    int id = index.data(CategoryModel::IdRole).toInt();
    if (id <= 0) return;

    FramelessInputDialog dlg("新建子分类", "请输入子分类名称:", "", this);
    if (dlg.exec() == QDialog::Accepted) {
        QString text = dlg.text();
        if (!text.isEmpty()) {
            Category cat;
            cat.name = text.toStdWString();
            cat.parentId = id;
            cat.color = L"#3498db";

            QSet<int> expandedIds;
            QStringList expandedNames;
            saveExpandedState(m_categoryTree, QModelIndex(), expandedIds, expandedNames);
            expandedIds.insert(id);

            CategoryRepo::add(cat);
            m_categoryModel->refresh();

            restoreExpandedState(m_categoryTree, QModelIndex(), expandedIds, expandedNames);
        }
    }
}

void CategoryPanel::onAddData() {
    QModelIndex index = m_categoryTree->currentIndex();
    if (!index.isValid()) return;
    int id = index.data(CategoryModel::IdRole).toInt();
    if (id <= 0) return;

    FramelessInputDialog dlg("新建数据", "请输入文件/笔记名称:", "", this);
    if (dlg.exec() == QDialog::Accepted) {
        QString name = dlg.text();
        if (!name.isEmpty()) {
            // [BUSINESS LOGIC] 暂时在本地数据库建立一个虚拟项并关联到此分类
            // 后续由 CoreController 负责物理文件同步
            std::wstring wname = name.toStdWString();
            CategoryRepo::addItemToCategory(id, L"new://" + wname);
            m_categoryModel->refresh();
            ToolTipOverlay::instance()->showText(QCursor::pos(), QString("已新建并归类: %1").arg(name), 1000);
        }
    }
}

void CategoryPanel::onClassifyToCategory() {
    QModelIndex index = m_categoryTree->currentIndex();
    if (!index.isValid()) return;
    int id = index.data(CategoryModel::IdRole).toInt();
    if (id <= 0) return;

    QSettings settings("ArcMeta团队", "ArcMeta");
    settings.setValue("Category/ExtensionTargetId", id);

    QSet<int> expandedIds;
    QStringList expandedNames;
    saveExpandedState(m_categoryTree, QModelIndex(), expandedIds, expandedNames);

    m_categoryModel->refresh();

    restoreExpandedState(m_categoryTree, QModelIndex(), expandedIds, expandedNames);

    QString name = index.data(CategoryModel::NameRole).toString();
    ToolTipOverlay::instance()->showText(QCursor::pos(), QString("已设为归类目标: %1").arg(name), 1000);
}

void CategoryPanel::onSetColor() {
    QModelIndex index = m_categoryTree->currentIndex();
    if (!index.isValid()) return;
    int id = index.data(CategoryModel::IdRole).toInt();
    if (id <= 0) return;

    auto all = CategoryRepo::getAll();
    for(auto& cat : all) {
        if(cat.id == id) {
            cat.color = L"#3498db"; 
            CategoryRepo::update(cat);
            break;
        }
    }
    
    QSet<int> expandedIds;
    QStringList expandedNames;
    saveExpandedState(m_categoryTree, QModelIndex(), expandedIds, expandedNames);

    m_categoryModel->refresh();

    restoreExpandedState(m_categoryTree, QModelIndex(), expandedIds, expandedNames);
    
    ToolTipOverlay::instance()->showText(QCursor::pos(), "分类颜色已重置为默认蓝", 1000);
}

void CategoryPanel::onRandomColor() {
    QModelIndex index = m_categoryTree->currentIndex();
    if (!index.isValid()) return;
    int id = index.data(CategoryModel::IdRole).toInt();
    if (id <= 0) return;
    
    QStringList colors = {"#3498db", "#2ecc71", "#e74c3c", "#f1c40f", "#9b59b6", "#1abc9c", "#e67e22", "#E24B4A", "#EF9F27", "#FAC775", "#639922", "#1D9E75", "#378ADD", "#7F77DD", "#62BAC1", "#F2B705", "#E91E63", "#FF551C"};
    QString color = colors[QRandomGenerator::global()->bounded(static_cast<int>(colors.size()))];
    
    auto all = CategoryRepo::getAll();
    for(auto& cat : all) {
        if(cat.id == id) {
            cat.color = color.toStdWString();
            CategoryRepo::update(cat);
            break;
        }
    }
    
    QSet<int> expandedIds;
    QStringList expandedNames;
    saveExpandedState(m_categoryTree, QModelIndex(), expandedIds, expandedNames);

    m_categoryModel->refresh();

    restoreExpandedState(m_categoryTree, QModelIndex(), expandedIds, expandedNames);
}

void CategoryPanel::onSetPresetTags() {
    QModelIndex index = m_categoryTree->currentIndex();
    if (!index.isValid()) return;
    int id = index.data(CategoryModel::IdRole).toInt();
    if (id <= 0) return;

    auto all = CategoryRepo::getAll();
    Category current;
    for(auto& c : all) if(c.id == id) { current = c; break; }

    QString initial;
    for(const auto& t : current.presetTags) initial += QString::fromStdWString(t) + ",";
    if (initial.endsWith(",")) initial.chop(1);

    FramelessInputDialog dlg("设置预设标签", "请输入标签 (用逗号分隔):", initial, this);
    if (dlg.exec() == QDialog::Accepted) {
        QStringList tags = dlg.text().split(QRegularExpression("[,，]"), Qt::SkipEmptyParts);
        current.presetTags.clear();
        for(const QString& t : tags) current.presetTags.push_back(t.trimmed().toStdWString());
        
        CategoryRepo::update(current);
        m_categoryModel->refresh();
        ToolTipOverlay::instance()->showText(QCursor::pos(), "预设标签已更新", 1000);
    }
}

void CategoryPanel::onTogglePin() {
    QModelIndex index = m_categoryTree->currentIndex();
    if (!index.isValid()) return;
    int id = index.data(CategoryModel::IdRole).toInt();
    if (id <= 0) return;
    bool isPinned = index.data(CategoryModel::PinnedRole).toBool();

    auto all = CategoryRepo::getAll();
    for(auto& cat : all) {
        if(cat.id == id) {
            cat.pinned = !isPinned;
            CategoryRepo::update(cat);
            break;
        }
    }

    QSet<int> expandedIds;
    QStringList expandedNames;
    saveExpandedState(m_categoryTree, QModelIndex(), expandedIds, expandedNames);

    m_categoryModel->refresh();

    restoreExpandedState(m_categoryTree, QModelIndex(), expandedIds, expandedNames);
}

void CategoryPanel::onSetPassword() {
    QModelIndex index = m_categoryTree->currentIndex();
    if (!index.isValid()) return;
    int id = index.data(CategoryModel::IdRole).toInt();
    if (id <= 0) return;

    FramelessInputDialog dlg("加密分类", "请输入访问密码:", "", this);
    // 物理还原：密码输入框应设为密码模式
    QLineEdit* edit = dlg.findChild<QLineEdit*>();
    if (edit) edit->setEchoMode(QLineEdit::Password);

    if (dlg.exec() == QDialog::Accepted) {
        QString pwd = dlg.text();
        if (!pwd.isEmpty()) {
            auto all = CategoryRepo::getAll();
            for(auto& cat : all) {
                if(cat.id == id) {
                    cat.encrypted = true;
                    // [SECURE] 实际项目中应存储加盐哈希，此处先完成 UI 逻辑链路
                    CategoryRepo::update(cat);
                    break;
                }
            }
            m_categoryModel->refresh();
            ToolTipOverlay::instance()->showText(QCursor::pos(), "分类已加密", 1000);
        }
    }
}

void CategoryPanel::onClearPassword() {
    QModelIndex index = m_categoryTree->currentIndex();
    if (!index.isValid()) return;
    int id = index.data(CategoryModel::IdRole).toInt();
    if (id <= 0) return;

    auto all = CategoryRepo::getAll();
    for(auto& cat : all) {
        if(cat.id == id) {
            cat.encrypted = false;
            CategoryRepo::update(cat);
            break;
        }
    }

    QSet<int> expandedIds;
    QStringList expandedNames;
    saveExpandedState(m_categoryTree, QModelIndex(), expandedIds, expandedNames);

    m_categoryModel->refresh();

    restoreExpandedState(m_categoryTree, QModelIndex(), expandedIds, expandedNames);
}

void CategoryPanel::onRenameCategory() {
    QModelIndex index = m_categoryTree->currentIndex();
    if (index.isValid() && index.data(CategoryModel::IdRole).toInt() > 0) {
        m_categoryTree->edit(index);
    }
}

void CategoryPanel::onDeleteCategory() {
    QModelIndex index = m_categoryTree->currentIndex();
    if (index.isValid()) {
        int id = index.data(CategoryModel::IdRole).toInt();
        if (id > 0) {
            QSet<int> expandedIds;
            QStringList expandedNames;
            saveExpandedState(m_categoryTree, QModelIndex(), expandedIds, expandedNames);

            ArcMeta::CategoryRepo::remove(id);
            m_categoryModel->refresh();

            restoreExpandedState(m_categoryTree, QModelIndex(), expandedIds, expandedNames);
        }
    }
}

void CategoryPanel::setFocusHighlight(bool visible) {
    if (m_focusLine) m_focusLine->setVisible(visible);
}

void CategoryPanel::initUi() {
    // 物理还原：1px 翠绿高亮焦点线 (#2ecc71)
    m_focusLine = new QWidget(this);
    m_focusLine->setFixedHeight(1);
    m_focusLine->setStyleSheet("background-color: #2ecc71;");
    m_focusLine->hide(); // 初始隐藏
    m_mainLayout->addWidget(m_focusLine);

    // 1. 标题栏
    QWidget* header = new QWidget(this);
    header->setObjectName("ContainerHeader");
    header->setFixedHeight(32);
    header->setStyleSheet(
        "QWidget#ContainerHeader {"
        "  background-color: #252526;"
        "  border-bottom: 1px solid #333;"
        "}"
    );
    QHBoxLayout* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(15, 2, 15, 0);
    headerLayout->setSpacing(8);

    QLabel* iconLabel = new QLabel(header);
    iconLabel->setPixmap(UiHelper::getIcon("category", QColor("#3498db"), 18).pixmap(18, 18));
    headerLayout->addWidget(iconLabel);

    QLabel* titleLabel = new QLabel("数据分类", header);
    titleLabel->setStyleSheet("font-size: 13px; font-weight: bold; color: #3498db; background: transparent; border: none;");
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    m_mainLayout->addWidget(header);

    // 2. 内容区包裹容器 (物理还原 8, 8, 8, 8 呼吸边距)
    QWidget* sbContent = new QWidget(this);
    sbContent->setStyleSheet("background: transparent; border: none;");
    auto* sbContentLayout = new QVBoxLayout(sbContent);
    sbContentLayout->setContentsMargins(8, 8, 8, 8);
    sbContentLayout->setSpacing(0);

    QString treeStyle = R"(
        QTreeView { background-color: transparent; border: none; color: #CCC; outline: none; }
        
        /* 物理还原：复原三角形折叠图标 (资源系统路径) */
        QTreeView::branch:has-children:closed { 
            image: url(:/icons/arrow_right.svg); 
        }
        QTreeView::branch:has-children:open { 
            image: url(:/icons/arrow_down.svg); 
        }
        QTreeView::branch:has-children:closed:has-siblings { 
            image: url(:/icons/arrow_right.svg); 
        }
        QTreeView::branch:has-children:open:has-siblings { 
            image: url(:/icons/arrow_down.svg); 
        }

        QTreeView::item { height: 26px; padding-left: 0px; }
    )";

    // 物理还原：单树架构，合并系统项与用户分类
    m_categoryTree = new DropTreeView(this);
    m_categoryTree->setStyleSheet(treeStyle); 
    m_categoryTree->setItemDelegate(new CategoryDelegate(this));
    
    // 使用 Both 模式同时加载系统项与用户分类
    m_categoryModel = new CategoryModel(CategoryModel::Both, this);
    m_categoryTree->setModel(m_categoryModel);
    
    m_categoryTree->setHeaderHidden(true);
    m_categoryTree->setRootIsDecorated(true);
    m_categoryTree->setIndentation(20);
    m_categoryTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_categoryTree->setDragEnabled(true);
    m_categoryTree->setAcceptDrops(true);
    m_categoryTree->setDropIndicatorShown(true);
    m_categoryTree->setDragDropMode(QAbstractItemView::InternalMove);
    m_categoryTree->setDefaultDropAction(Qt::MoveAction);
    m_categoryTree->setSelectionMode(QAbstractItemView::ExtendedSelection);

    // 默认展开“我的分类”
    for (int i = 0; i < m_categoryModel->rowCount(); ++i) {
        QModelIndex idx = m_categoryModel->index(i, 0);
        if (idx.data(CategoryModel::NameRole).toString() == "我的分类") {
            m_categoryTree->setExpanded(idx, true);
        }
    }

    connect(m_categoryTree, &QTreeView::clicked, [this](const QModelIndex& index) {
        QString type = index.data(CategoryModel::TypeRole).toString();
        QString name = index.data(CategoryModel::NameRole).toString();
        // 只有具体分类或系统项触发选中事件
        if (type == "category" || type != "") {
             emit categorySelected(name);
        }
    });

    connect(m_categoryTree, &DropTreeView::pathsDropped, [this](const QStringList& paths, const QModelIndex& index) {
        if (!index.isValid()) return;
        
        int categoryId = index.data(CategoryModel::IdRole).toInt();
        QString itemType = index.data(CategoryModel::TypeRole).toString();
        QString itemName = index.data(CategoryModel::NameRole).toString();
        
        int count = 0;
        bool changed = false;

        // 2026-03-xx 按照用户要求：实现全局数据库持久化收藏与归类逻辑
        if (categoryId > 0) {
            // 分支 A：拖拽至具体自定义分类项
            for (const QString& path : paths) {
                if (CategoryRepo::addItemToCategory(categoryId, path.toStdWString())) {
                    count++;
                }
            }
            changed = (count > 0);
            if (changed) {
                ToolTipOverlay::instance()->showText(QCursor::pos(), 
                    QString("<b style='color:#2ecc71;'>已归类 %1 个项目到 [%2]</b>").arg(count).arg(itemName), 1200, QColor("#2ecc71"));
            }
        } 
        else if (itemType == "bookmark") {
            // 分支 B：拖拽至系统预设的“收藏”项
            for (const QString& path : paths) {
                // 2026-03-xx 物理强化：强制使用原生分隔符，确保数据库路径匹配成功
                QString normPath = QDir::toNativeSeparators(path);
                // 物理写入：更新 items 表的 pinned 状态
                MetadataManager::instance().setPinned(normPath.toStdWString(), true);
                count++;
            }
            changed = (count > 0);
            if (changed) {
                ToolTipOverlay::instance()->showText(QCursor::pos(), 
                    QString("<b style='color:#2ecc71;'>已成功收藏 %1 个项目</b>").arg(count), 1200, QColor("#2ecc71"));
            }
        }
        
        if (changed) {
            // 物理联动：强制刷新模型以触发 CategoryRepo::getCounts/getSystemCounts 重新统计
            // 确保侧边栏名称后的 (数量) 实时更新并固化
            m_categoryModel->refresh();
        }
    });
    
    sbContentLayout->addWidget(m_categoryTree);
    m_mainLayout->addWidget(sbContent, 1);
}

bool CategoryPanel::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            // [UX] 两段式：查找对话框内的第一个非空输入框
            QLineEdit* edit = findChild<QLineEdit*>();
            if (edit && !edit->text().isEmpty()) {
                edit->clear();
                return true;
            }
        }
    }
    return QFrame::eventFilter(obj, event);
}

} // namespace ArcMeta
