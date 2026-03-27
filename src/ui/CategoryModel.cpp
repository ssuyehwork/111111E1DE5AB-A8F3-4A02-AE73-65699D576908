#include "CategoryModel.h"
#include <QMimeData>

namespace ArcMeta {

CategoryModel::CategoryModel(QObject* parent) : QStandardItemModel(parent) {
    refresh();
}

void CategoryModel::refresh() {
    clear();
    QList<Category> all = CategoryRepo::getAll();
    loadSubCategories(invisibleRootItem(), 0, all);
}

void CategoryModel::loadSubCategories(QStandardItem* parentItem, int parentId, const QList<Category>& allCategories) {
    for (const auto& cat : allCategories) {
        if (cat.parentId == parentId) {
            auto* item = new QStandardItem(cat.name);
            item->setData(cat.id, IdRole);
            item->setData(cat.parentId, ParentIdRole);
            item->setData(cat.color, ColorRole);
            item->setData(cat.presetTags, TagsRole);
            item->setEditable(true);

            parentItem->appendRow(item);
            loadSubCategories(item, cat.id, allCategories);
        }
    }
}

Qt::DropActions CategoryModel::supportedDropActions() const {
    return Qt::MoveAction;
}

QStringList CategoryModel::mimeTypes() const {
    return {"application/x-arcmeta-category"};
}

QMimeData* CategoryModel::mimeData(const QModelIndexList& indexes) const {
    QMimeData* mimeData = new QMimeData();
    QByteArray data;
    // 简单序列化 ID
    for (const auto& idx : indexes) {
        data.append(QString::number(idx.data(IdRole).toInt()).toUtf8() + ",");
    }
    mimeData->setData("application/x-arcmeta-category", data);
    return mimeData;
}

bool CategoryModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) {
    Q_UNUSED(action); Q_UNUSED(row); Q_UNUSED(column);
    if (!data->hasFormat("application/x-arcmeta-category")) return false;

    int newParentId = parent.isValid() ? parent.data(IdRole).toInt() : 0;
    QStringList ids = QString(data->data("application/x-arcmeta-category")).split(',', Qt::SkipEmptyParts);

    for (const QString& idStr : ids) {
        int id = idStr.toInt();
        // 简单更新数据库中的 parentId
        // 这里需要 CategoryRepo 支持更新 parentId，由于目前 Repo 较简单，先预留
    }

    refresh();
    return true;
}

} // namespace ArcMeta
