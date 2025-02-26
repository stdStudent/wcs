#ifndef CRYPTHELPER_H
#define CRYPTHELPER_H

#include <Windows.h>
#include <wincrypt.h>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>

class CryptHelper {
private:
    HCRYPTPROV m_hCryptProv = NULL;
    HCRYPTKEY m_hKeyPair = NULL;
    DWORD m_dwKeySize = 2048;
    std::string m_publicKeyBlob;
    std::string m_privateKeyBlob;
    bool m_hasKeyPair = false;

    // Helper function to convert error code to string
    std::string getLastErrorAsString() {
        const DWORD errorMessageID = ::GetLastError();
        if (errorMessageID == 0)
            return "No error";

        LPWSTR messageBuffer = nullptr;
        const size_t size = FormatMessageW(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ARGUMENT_ARRAY | FORMAT_MESSAGE_ALLOCATE_BUFFER,
            nullptr,
            errorMessageID,
            MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
            reinterpret_cast<LPWSTR>(&messageBuffer),
            0,
            nullptr
        );

        std::wstring message(messageBuffer, size);
        LocalFree(messageBuffer);

        return { message.begin(), message.end() };
    }

    // Helper to export key to blob
    bool exportKeyToBlob(const HCRYPTKEY hKey, const DWORD dwBlobType, std::string& blobOut, const HCRYPTKEY hPubKey = 0) {
        DWORD dwBlobLen = 0;

        // Get the required buffer size
        if (!CryptExportKey(hKey, hPubKey, dwBlobType, 0, nullptr, &dwBlobLen))
            return false;

        // Allocate buffer and export key
        std::vector<BYTE> blobData(dwBlobLen);
        if (!CryptExportKey(hKey, hPubKey, dwBlobType, 0, blobData.data(), &dwBlobLen))
            return false;

        blobOut.assign(reinterpret_cast<char*>(blobData.data()), dwBlobLen);
        return true;
    }

public:
    CryptHelper() = default;

    ~CryptHelper() {
        cleanup();
    }

    // Initialize cryptography provider
    bool initialize() {
        if (m_hCryptProv)
            return true; // Already initialized

        // Use the AES Cryptographic Provider which supports AES algorithms
        if (!CryptAcquireContext(&m_hCryptProv, nullptr, MS_ENH_RSA_AES_PROV, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
            // Try to create a new key container if it doesn't exist
            if (GetLastError() == NTE_BAD_KEYSET) {
                if (!CryptAcquireContext(&m_hCryptProv, nullptr, MS_ENH_RSA_AES_PROV, PROV_RSA_AES, CRYPT_NEWKEYSET))
                    throw std::runtime_error("Failed to acquire cryptography context: " + getLastErrorAsString());
            } else
                throw std::runtime_error("Failed to acquire cryptography context: " + getLastErrorAsString());

        return true;
    }

    // Generate a new RSA key pair
    bool generateKeyPair(DWORD dwKeySize = 2048) {
        if (!m_hCryptProv && !initialize())
            return false;

        // Release any existing key
        if (m_hKeyPair) {
            CryptDestroyKey(m_hKeyPair);
            m_hKeyPair = NULL;
        }

        m_dwKeySize = dwKeySize;

        // Generate the key pair
        if (!CryptGenKey(m_hCryptProv, AT_KEYEXCHANGE, m_dwKeySize << 16 | CRYPT_EXPORTABLE, &m_hKeyPair))
            throw std::runtime_error("Failed to generate key pair: " + getLastErrorAsString());

        // Export the public key
        if (!exportKeyToBlob(m_hKeyPair, PUBLICKEYBLOB, m_publicKeyBlob))
            throw std::runtime_error("Failed to export public key: " + getLastErrorAsString());

        // Export the private key
        if (!exportKeyToBlob(m_hKeyPair, PRIVATEKEYBLOB, m_privateKeyBlob))
            throw std::runtime_error("Failed to export private key: " + getLastErrorAsString());

        m_hasKeyPair = true;
        return true;
    }

    // Get the public key blob
    std::string getPublicKeyBlob() const {
        if (!m_hasKeyPair)
            throw std::runtime_error("No key pair has been generated");

        return m_publicKeyBlob;
    }

    // Get the private key blob (should be kept secure!)
    std::string getPrivateKeyBlob() const {
        if (!m_hasKeyPair)
            throw std::runtime_error("No key pair has been generated");

        return m_privateKeyBlob;
    }

    // Import a public key from blob
    HCRYPTKEY importPublicKey(const std::string& publicKeyBlob) {
        if (!m_hCryptProv && !initialize())
            return NULL;

        HCRYPTKEY hPubKey = NULL;
        if (
            !CryptImportKey(
                m_hCryptProv,
               reinterpret_cast<const BYTE*>(publicKeyBlob.data()),
               static_cast<DWORD>(publicKeyBlob.size()),
               0, 0, &hPubKey
            )
        ) throw std::runtime_error("Failed to import public key: " + getLastErrorAsString());

        return hPubKey;
    }

    // Import a private key from blob
    HCRYPTKEY importPrivateKey(const std::string& privateKeyBlob) {
        if (!m_hCryptProv && !initialize())
            return NULL;

        HCRYPTKEY hPrivKey = NULL;
        if (
            !CryptImportKey(
                m_hCryptProv,
                reinterpret_cast<const BYTE *>(privateKeyBlob.data()),
                static_cast<DWORD>(privateKeyBlob.size()),
                0,
                0,
                &hPrivKey
            )
        ) throw std::runtime_error("Failed to import private key: " + getLastErrorAsString());

        return hPrivKey;
    }

    // Encrypt data using a public key
    std::vector<BYTE> encryptData(const HCRYPTKEY hKey, const std::vector<BYTE>& data) {
        if (!m_hCryptProv && !initialize())
            throw std::runtime_error("Cryptography provider not initialized");

        // Make a copy of the data since CryptEncrypt modifies it
        std::vector<BYTE> encryptedData = data;

        // Get the required buffer size
        auto dwDataLen = static_cast<DWORD>(encryptedData.size());
        DWORD dwBufLen = dwDataLen;

        if (!CryptEncrypt(hKey, 0, TRUE, 0, nullptr, &dwBufLen, 0))
            throw std::runtime_error("Failed to get encrypted data length: " + getLastErrorAsString());

        // Resize buffer to required size
        encryptedData.resize(dwBufLen);

        // Encrypt the data
        dwDataLen = static_cast<DWORD>(data.size());
        if (!CryptEncrypt(hKey, 0, TRUE, 0, encryptedData.data(), &dwDataLen, static_cast<DWORD>(encryptedData.size())))
            throw std::runtime_error("Failed to encrypt data: " + getLastErrorAsString());

        encryptedData.resize(dwDataLen);
        return encryptedData;
    }

    // Decrypt data using a private key
    std::vector<BYTE> decryptData(const HCRYPTKEY hKey, const std::vector<BYTE>& encryptedData) {
        if (!m_hCryptProv && !initialize())
            throw std::runtime_error("Cryptography provider not initialized");

        // Make a copy of the data since CryptDecrypt modifies it
        std::vector<BYTE> decryptedData = encryptedData;

        // Get the buffer size
        auto dwDataLen = static_cast<DWORD>(decryptedData.size());

        // Decrypt the data
        if (!CryptDecrypt(hKey, 0, TRUE, 0, decryptedData.data(), &dwDataLen))
            throw std::runtime_error("Failed to decrypt data: " + getLastErrorAsString());

        decryptedData.resize(dwDataLen);
        return decryptedData;
    }

    // Create a hash of the data (for HMAC)
    std::vector<BYTE> createHash(const std::vector<BYTE>& data, const ALG_ID algId = CALG_SHA_256) {
        if (!m_hCryptProv && !initialize())
            throw std::runtime_error("Cryptography provider not initialized");

        HCRYPTHASH hHash = NULL;
        if (!CryptCreateHash(m_hCryptProv, algId, 0, 0, &hHash))
            throw std::runtime_error("Failed to create hash object: " + getLastErrorAsString());

        std::vector<BYTE> hashValue;

        // Add data to the hash
        if (!CryptHashData(hHash, data.data(), static_cast<DWORD>(data.size()), 0)) {
            CryptDestroyHash(hHash);
            throw std::runtime_error("Failed to hash data: " + getLastErrorAsString());
        }

        // Get hash value
        DWORD dwHashLen = 0;
        DWORD dwHashLenSize = sizeof(DWORD);

        if (!CryptGetHashParam(hHash, HP_HASHSIZE, reinterpret_cast<BYTE *>(&dwHashLen), &dwHashLenSize, 0)) {
            CryptDestroyHash(hHash);
            throw std::runtime_error("Failed to get hash size: " + getLastErrorAsString());
        }

        hashValue.resize(dwHashLen);

        if (!CryptGetHashParam(hHash, HP_HASHVAL, hashValue.data(), &dwHashLen, 0)) {
            CryptDestroyHash(hHash);
            throw std::runtime_error("Failed to get hash value: " + getLastErrorAsString());
        }

        CryptDestroyHash(hHash);
        return hashValue;
    }

    // Generate a symmetric session key
    HCRYPTKEY generateSessionKey(ALG_ID algId = CALG_AES_256) {
        if (!m_hCryptProv && !initialize())
            throw std::runtime_error("Cryptography provider not initialized");

        HCRYPTKEY hSessionKey = NULL;
        if (!CryptGenKey(m_hCryptProv, algId, CRYPT_EXPORTABLE, &hSessionKey))
            throw std::runtime_error("Failed to generate session key: " + getLastErrorAsString());

        return hSessionKey;
    }

    // Export a session key
    std::string exportSessionKey(const HCRYPTKEY hSessionKey, const HCRYPTKEY hRecipientPublicKey) {
        std::string keyBlob;

        if (!exportKeyToBlob(hSessionKey, SIMPLEBLOB, keyBlob, hRecipientPublicKey)) {
            throw std::runtime_error("Failed to export session key: " + getLastErrorAsString());
        }

        return keyBlob;
    }

    // Cleanup resources
    void cleanup() {
        if (m_hKeyPair) {
            CryptDestroyKey(m_hKeyPair);
            m_hKeyPair = NULL;
        }

        if (m_hCryptProv) {
            CryptReleaseContext(m_hCryptProv, 0);
            m_hCryptProv = NULL;
        }

        m_hasKeyPair = false;
    }
};

#endif //CRYPTHELPER_H
