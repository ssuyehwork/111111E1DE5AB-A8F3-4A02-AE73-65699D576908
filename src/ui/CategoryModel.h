#ifndef CATEGORY_MODEL_H
#define CATEGORY_MODEL_H

#include <QStandardItemModel>
#include "../db/CategoryRepo.h"

namespace ArcMeta {

class CategoryModel : public QStandardItemModel {
    Q_OBJECT
public:
    explicit CategoryModel(QObject* parent = nullptr);

    void refresh();

    // 角色定义
    enum Roles {
        IdRole = Qt::UserRole + 1,
        ParentIdRole,
        ColorRole,
        TagsRole
    };

    // 拖拽支持
    Qt::DropActions supportedDropActions() const override;
    QStringList mimeTypes() const override;
    QMimeData* mimeData(const QModelIndexList& indexes) const override;
    bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) override;

private:
    void loadSubCategories(QStandardItem* parentItem, int parentId, const QList<Category>& allCategories);
};

} // namespace ArcMeta

#endif // CATEGORY_MODEL_H
