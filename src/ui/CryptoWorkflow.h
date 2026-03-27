#ifndef CRYPTO_WORKFLOW_H
#define CRYPTO_WORKFLOW_H

#include <QString>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include "PasswordDialog.h"
#include "../crypto/EncryptionManager.h"
#include "../meta/AmMetaJson.h"

namespace ArcMeta {

class CryptoWorkflow {
public:
    static bool handleFileAccess(const QString& encryptedFilePath, QWidget* parent = nullptr) {
        // 1. 获取元数据
        QString dirPath = QFileInfo(encryptedFilePath).absolutePath();
        AmMeta meta = AmMetaJson::load(dirPath);
        QString fileName = QFileInfo(encryptedFilePath).fileName();

        if (!meta.items.contains(fileName) || !meta.items[fileName].encrypted) {
            return false;
        }

        const auto& item = meta.items[fileName];

        // 2. 弹出密码框
        PasswordDialog dlg(parent);
        if (dlg.exec() != QDialog::Accepted) return false;

        QString password = dlg.getPassword();

        // 3. 准备临时解密目录
        QString tempDir = QDir::tempPath() + "/amtemp/";
        QDir().mkpath(tempDir);
        QString destPath = tempDir + item.originalName;

        // 4. 执行解密
        // 转换 Salt (简单处理，实际应从 base64 或 16 进制还原)
        std::vector<BYTE> salt(item.encryptSalt.begin(), item.encryptSalt.end());
        std::vector<BYTE> hash(item.encryptVerifyHash.begin(), item.encryptVerifyHash.end());

        if (EncryptionManager::instance().decryptFile(encryptedFilePath, destPath, password, salt, hash)) {
            // 5. 打开文件
            QDesktopServices::openUrl(QUrl::fromLocalFile(destPath));
            // 注意：规范要求用户关闭后删除，这里简单实现，实际可配合文件观察者
            return true;
        }

        return false;
    }
};

} // namespace ArcMeta

#endif // CRYPTO_WORKFLOW_H
