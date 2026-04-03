#pragma once

#include <QStandardItemModel>

namespace ArcMeta {

class CategoryModel : public QStandardItemModel {
    Q_OBJECT
public:
    enum Type { System, User, Both };
    enum Roles {
        TypeRole = Qt::UserRole,
        IdRole,
        ColorRole,
        NameRole,
        PinnedRole,
        PathRole,
        EncryptedRole
    };
    explicit CategoryModel(Type type, QObject* parent = nullptr);
public slots:
    void refresh();

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& val, int role = Qt::EditRole) override;

    Qt::DropActions supportedDropActions() const override;
    bool dropMimeData(const QMimeData* mimeData, Qt::DropAction action, int row, int column, const QModelIndex& parent) override;

private:
    Type m_type;
};

} // namespace ArcMeta
