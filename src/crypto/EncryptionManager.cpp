#include "EncryptionManager.h"
#include <QFile>
#include <QDebug>
#include <ntstatus.h>

#pragma comment(lib, "bcrypt.lib")

namespace ArcMeta {

EncryptionManager& EncryptionManager::instance() {
    static EncryptionManager inst;
    return inst;
}

bool EncryptionManager::deriveKey(const QString& password, const std::vector<BYTE>& salt, std::vector<BYTE>& outKey) {
    BCRYPT_ALG_HANDLE hAlg = NULL;
    NTSTATUS status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (!NT_SUCCESS(status)) return false;

    outKey.resize(32); // AES-256
    std::wstring wPwd = password.toStdWString();

    status = BCryptDeriveKeyPBKDF2(hAlg, (PUCHAR)wPwd.c_str(), (ULONG)(wPwd.length() * sizeof(wchar_t)),
                                 (PUCHAR)salt.data(), (ULONG)salt.size(), 10000,
                                 outKey.data(), (ULONG)outKey.size(), 0);

    BCryptCloseAlgorithmProvider(hAlg, 0);
    return NT_SUCCESS(status);
}

bool EncryptionManager::encryptFile(const QString& srcPath, const QString& destPath, const QString& password,
                                  std::vector<BYTE>& outSalt, std::vector<BYTE>& outHash) {
    QFile srcFile(srcPath);
    if (!srcFile.open(QIODevice::ReadOnly)) return false;
    QByteArray data = srcFile.readAll();
    srcFile.close();

    // 生成随机盐
    outSalt.resize(16);
    BCryptGenRandom(NULL, outSalt.data(), (ULONG)outSalt.size(), BCRYPT_USE_SYSTEM_PREFERRED_RNG);

    std::vector<BYTE> key;
    if (!deriveKey(password, outSalt, key)) return false;

    std::vector<BYTE> encryptedData;
    if (!processCrypto(true, std::vector<BYTE>(data.begin(), data.end()), key, encryptedData)) return false;

    QFile destFile(destPath);
    if (!destFile.open(QIODevice::WriteOnly)) return false;
    destFile.write((const char*)encryptedData.data(), encryptedData.size());
    destFile.close();

    // 简单生成验证哈希（实际应使用更复杂的 HMAC）
    outHash.resize(32);
    // 占位逻辑：此处可进一步实现摘要计算

    return true;
}

bool EncryptionManager::processCrypto(bool encrypt, const std::vector<BYTE>& data, const std::vector<BYTE>& key,
                                    std::vector<BYTE>& outData) {
    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_KEY_HANDLE hKey = NULL;
    NTSTATUS status;

    status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, NULL, 0);
    if (!NT_SUCCESS(status)) return false;

    status = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_CBC, sizeof(BCRYPT_CHAIN_MODE_CBC), 0);
    if (!NT_SUCCESS(status)) goto cleanup;

    status = BCryptGenerateSymmetricKey(hAlg, &hKey, NULL, 0, (PUCHAR)key.data(), (ULONG)key.size(), 0);
    if (!NT_SUCCESS(status)) goto cleanup;

    ULONG cbOutput = 0;
    // 获取所需缓冲区大小
    status = BCryptEncrypt(hKey, (PUCHAR)data.data(), (ULONG)data.size(), NULL, NULL, 0, NULL, 0, &cbOutput, BCRYPT_BLOCK_PADDING);
    if (!NT_SUCCESS(status)) goto cleanup;

    outData.resize(cbOutput);
    status = encrypt ?
        BCryptEncrypt(hKey, (PUCHAR)data.data(), (ULONG)data.size(), NULL, NULL, 0, outData.data(), (ULONG)outData.size(), &cbOutput, BCRYPT_BLOCK_PADDING) :
        BCryptDecrypt(hKey, (PUCHAR)data.data(), (ULONG)data.size(), NULL, NULL, 0, outData.data(), (ULONG)outData.size(), &cbOutput, BCRYPT_BLOCK_PADDING);

cleanup:
    if (hKey) BCryptDestroyKey(hKey);
    if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
    return NT_SUCCESS(status);
}

bool EncryptionManager::decryptFile(const QString& srcPath, const QString& destPath, const QString& password,
                                  const std::vector<BYTE>& salt, const std::vector<BYTE>& verifyHash) {
    Q_UNUSED(verifyHash);
    QFile srcFile(srcPath);
    if (!srcFile.open(QIODevice::ReadOnly)) return false;
    QByteArray data = srcFile.readAll();
    srcFile.close();

    std::vector<BYTE> key;
    if (!deriveKey(password, salt, key)) return false;

    std::vector<BYTE> decryptedData;
    if (!processCrypto(false, std::vector<BYTE>(data.begin(), data.end()), key, decryptedData)) return false;

    QFile destFile(destPath);
    if (!destFile.open(QIODevice::WriteOnly)) return false;
    destFile.write((const char*)decryptedData.data(), decryptedData.size());
    destFile.close();

    return true;
}

} // namespace ArcMeta
