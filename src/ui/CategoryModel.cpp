#include "CategoryModel.h"
#include <QMimeData>
#include <QUrl>
#include <QFileInfo>
#include "../db/CategoryItemRepo.h"
#include "../mft/MftReader.h" // 假设我们可以通过某种方式获取 FRN

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
    return Qt::MoveAction | Qt::CopyAction;
}

QStringList CategoryModel::mimeTypes() const {
    return {"application/x-arcmeta-category", "text/uri-list"};
}

QMimeData* CategoryModel::mimeData(const QModelIndexList& indexes) const {
    QMimeData* mimeData = new QMimeData();
    QByteArray data;
    for (const auto& idx : indexes) {
        data.append(QString::number(idx.data(IdRole).toInt()).toUtf8() + ",");
    }
    mimeData->setData("application/x-arcmeta-category", data);
    return mimeData;
}

bool CategoryModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) {
    Q_UNUSED(action); Q_UNUSED(row); Q_UNUSED(column);

    int targetCategoryId = parent.isValid() ? parent.data(IdRole).toInt() : 0;
    if (targetCategoryId == 0) return false;

    // 情况 A: 内部目录树重组
    if (data->hasFormat("application/x-arcmeta-category")) {
        QStringList ids = QString(data->data("application/x-arcmeta-category")).split(',', Qt::SkipEmptyParts);
        QList<Category> all = CategoryRepo::getAll();
        for (const QString& idStr : ids) {
            int id = idStr.toInt();
            for (auto& cat : all) {
                if (cat.id == id) {
                    cat.parentId = targetCategoryId;
                    CategoryRepo::update(cat);
                    break;
                }
            }
        }
        refresh();
        return true;
    }

    // 情况 B: 从内容面板拖入文件 (text/uri-list)
    if (data->hasUrls()) {
        for (const QUrl& url : data->urls()) {
            QString path = url.toLocalFile();
            if (path.isEmpty()) continue;

            // 为了建立 FRN 关联，我们需要打开文件获取 FRN
            // 此处简化处理：通过 Windows API 获取 FRN
            HANDLE hFile = CreateFileW((LPCWSTR)path.utf16(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                      NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
            if (hFile != INVALID_HANDLE_VALUE) {
                BY_HANDLE_FILE_INFORMATION info;
                if (GetFileInformationByHandle(hFile, &info)) {
                    DWORDLONG frn = ((DWORDLONG)info.nFileIndexHigh << 32) | info.nFileIndexLow;
                    CategoryItemRepo::addItem(targetCategoryId, frn);
                }
                CloseHandle(hFile);
            }
        }
        return true;
    }

    return false;
}

} // namespace ArcMeta
