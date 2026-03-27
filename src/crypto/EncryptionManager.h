#ifndef ENCRYPTION_MANAGER_H
#define ENCRYPTION_MANAGER_H

#include <QString>
#include <vector>
#include <windows.h>
#include <bcrypt.h>

namespace ArcMeta {

class EncryptionManager {
public:
    static EncryptionManager& instance();

    // 派生密钥 (PBKDF2)
    bool deriveKey(const QString& password, const std::vector<BYTE>& salt, std::vector<BYTE>& outKey);

    // 加密文件
    bool encryptFile(const QString& srcPath, const QString& destPath, const QString& password,
                    std::vector<BYTE>& outSalt, std::vector<BYTE>& outHash);

    // 解密文件
    bool decryptFile(const QString& srcPath, const QString& destPath, const QString& password,
                    const std::vector<BYTE>& salt, const std::vector<BYTE>& verifyHash);

private:
    EncryptionManager() = default;
    ~EncryptionManager() = default;

    // 底层 AES-256-CBC 实现 (CNG API)
    bool processCrypto(bool encrypt, const std::vector<BYTE>& data, const std::vector<BYTE>& key,
                      std::vector<BYTE>& outData);
};

} // namespace ArcMeta

#endif // ENCRYPTION_MANAGER_H
