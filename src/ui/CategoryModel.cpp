#include "CategoryModel.h"
#include "../db/CategoryRepo.h"
#include "../db/ItemRepo.h"
#include "UiHelper.h"
#include <QMimeData>
#include <QFont>
#include <QTimer>
#include <QSet>

namespace ArcMeta {

CategoryModel::CategoryModel(Type type, QObject* parent)
    : QStandardItemModel(parent), m_type(type)
{
    refresh();
}

void CategoryModel::refresh() {
    clear();
    QStandardItem* root = invisibleRootItem();

    if (m_type == System || m_type == Both) {
        auto addSystemItem = [&](const QString& name, const QString& type, const QString& icon, const QString& color = "#aaaaaa") {
            // 目前由于底层接口限制，暂不显示实时统计数字，仅对齐架构
            QString display = name;
            QStandardItem* item = new QStandardItem(display);
            item->setData(type, TypeRole);
            item->setData(name, NameRole);
            item->setData(color, ColorRole);
            item->setEditable(false);
            item->setIcon(UiHelper::getIcon(icon, QColor(color), 16));
            root->appendRow(item);
        };

        addSystemItem("全部数据", "all", "all_data", "#3498db");
        addSystemItem("今日数据", "today", "today", "#2ecc71");
        addSystemItem("昨日数据", "yesterday", "today", "#f39c12");
        addSystemItem("最近访问", "recently_visited", "clock_history", "#9b59b6");
        addSystemItem("未分类", "uncategorized", "uncategorized", "#e67e22");
        addSystemItem("未标签", "untagged", "untagged", "#62BAC1");
        addSystemItem("收藏", "bookmark", "star", "#F2B705");
        addSystemItem("回收站", "trash", "trash", "#e74c3c");
    }

    if (m_type == User || m_type == Both) {
        auto categories = CategoryRepo::getAll();

        QStandardItem* userGroup = new QStandardItem("我的分类");
        userGroup->setData("我的分类", NameRole);
        userGroup->setSelectable(false);
        userGroup->setEditable(false);
        userGroup->setFlags(userGroup->flags() | Qt::ItemIsDropEnabled);
        userGroup->setIcon(UiHelper::getIcon("branch", QColor("#FFFFFF"), 16));

        QFont font = userGroup->font();
        font.setBold(true);
        userGroup->setFont(font);
        userGroup->setForeground(QColor("#FFFFFF"));

        root->appendRow(userGroup);

        QMap<int, QStandardItem*> itemMap;

        for (const auto& cat : categories) {
            int id = cat.id;
            QString name = QString::fromStdWString(cat.name);
            QString color = QString::fromStdWString(cat.color).isEmpty() ? "#aaaaaa" : QString::fromStdWString(cat.color);

            QStandardItem* item = new QStandardItem(name);
            item->setData("category", TypeRole);
            item->setData(id, IdRole);
            item->setData(color, ColorRole);
            item->setData(name, NameRole);
            item->setData(cat.pinned, PinnedRole);
            item->setFlags(item->flags() | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);

            if (cat.pinned) {
                item->setIcon(UiHelper::getIcon("pin", QColor(color), 16));
            } else {
                item->setIcon(UiHelper::getIcon("circle_filled", QColor(color), 14));
            }
            itemMap[id] = item;
        }

        for (const auto& cat : categories) {
            int id = cat.id;
            if (!itemMap.contains(id)) continue;

            int parentId = cat.parentId;
            if (parentId > 0 && itemMap.contains(parentId)) {
                itemMap[parentId]->appendRow(itemMap[id]);
            } else {
                userGroup->appendRow(itemMap[id]);
            }
        }
    }
}

QVariant CategoryModel::data(const QModelIndex& index, int role) const {
    if (role == Qt::EditRole) {
        return QStandardItemModel::data(index, NameRole);
    }
    return QStandardItemModel::data(index, role);
}

bool CategoryModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (role == Qt::EditRole) {
        QString newName = value.toString().trimmed();
        int id = index.data(IdRole).toInt();
        if (!newName.isEmpty() && id > 0) {
            // 获取分类并更新名称
            auto categories = CategoryRepo::getAll();
            for (auto& cat : categories) {
                if (cat.id == id) {
                    cat.name = newName.toStdWString();
                    CategoryRepo::update(cat);
                    break;
                }
            }
            QTimer::singleShot(0, [this]() { this->refresh(); });
            return true;
        }
        return false;
    }
    return QStandardItemModel::setData(index, value, role);
}

Qt::DropActions CategoryModel::supportedDropActions() const {
    return Qt::MoveAction;
}

bool CategoryModel::dropMimeData(const QMimeData* mimeData, Qt::DropAction action, int row, int column, const QModelIndex& parent) {
    Q_UNUSED(mimeData);
    Q_UNUSED(action);
    Q_UNUSED(row);
    Q_UNUSED(column);

    QModelIndex actualParent = parent;
    if (actualParent.isValid()) {
        QStandardItem* parentItem = itemFromIndex(actualParent);
        if (!parentItem) return false;
        QString name = parentItem->data(NameRole).toString();
        if (parentItem->data(TypeRole).toString() != "category" && name != "我的分类") {
            return false;
        }
    }
    return QStandardItemModel::dropMimeData(mimeData, action, row, column, actualParent);
}

} // namespace ArcMeta
