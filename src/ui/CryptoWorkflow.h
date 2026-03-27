#ifndef CRYPTO_WORKFLOW_H
#define CRYPTO_WORKFLOW_H

#include <QString>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include "PasswordDialog.h"
#include "../crypto/EncryptionManager.h"
#include "../meta/AmMetaJson.h"

namespace ArcMeta {

class CryptoWorkflow {
public:
    static void encryptFile(const QString& filePath, QWidget* parent = nullptr) {
        PasswordDialog dlg(true, parent);
        if (dlg.exec() != QDialog::Accepted) return;

        QString password = dlg.password();
        QString destPath = filePath + ".amenc";

        std::vector<BYTE> salt, hash;
        if (EncryptionManager::instance().encryptFile(filePath, destPath, password, salt, hash)) {
            // 写入元数据
            QFileInfo fi(filePath);
            AmMeta meta = AmMetaJson::load(fi.absolutePath());

            ItemMetadata item = meta.items.value(fi.fileName());
            item.encrypted = true;
            item.originalName = fi.fileName();
            // 简单处理：将字节数组存为 Base64
            item.encryptSalt = QString::fromLatin1(QByteArray((const char*)salt.data(), salt.size()).toBase64());
            item.encryptVerifyHash = QString::fromLatin1(QByteArray((const char*)hash.data(), hash.size()).toBase64());

            meta.items[fi.fileName()] = item;
            AmMetaJson::save(fi.absolutePath(), meta);

            QFile::remove(filePath);
            QMessageBox::information(parent, "成功", "文件已加密。");
        }
    }

    static void handleFileOpen(const QString& filePath, QWidget* parent = nullptr) {
        if (!filePath.endsWith(".amenc")) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
            return;
        }

        PasswordDialog dlg(false, parent);
        if (dlg.exec() != QDialog::Accepted) return;

        // 获取元数据中的 Salt 和 Hash
        QFileInfo fi(filePath);
        AmMeta meta = AmMetaJson::load(fi.absolutePath());
        ItemMetadata item = meta.items.value(fi.fileName());

        QByteArray salt = QByteArray::fromBase64(item.encryptSalt.toLatin1());
        QByteArray hash = QByteArray::fromBase64(item.encryptVerifyHash.toLatin1());

        QString tempPath = QDir::tempPath() + "/amtemp/" + item.originalName;
        QDir().mkpath(QFileInfo(tempPath).absolutePath());

        if (EncryptionManager::instance().decryptFile(filePath, tempPath, dlg.password(),
            std::vector<BYTE>(salt.begin(), salt.end()), std::vector<BYTE>(hash.begin(), hash.end()))) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(tempPath));
        } else {
            QMessageBox::critical(parent, "错误", "密码错误。");
        }
    }
};

} // namespace ArcMeta

#endif // CRYPTO_WORKFLOW_H
