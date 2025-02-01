#pragma once
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <mutex>

/**
 * SecurityManager handles NWK & APS encryption, key storage, frame counters.
 * In a real Zigbee 3.0 stack, you'd do AES-CCM* with frame counters, store them persistently.
 */
class SecurityManager
{
public:
    SecurityManager();
    ~SecurityManager();

    // NWK encryption/decryption
    bool nwkEncrypt(const std::vector<uint8_t> &plaintext,
                    uint32_t &txFrameCounter,
                    std::vector<uint8_t> &ciphertext);

    bool nwkDecrypt(std::vector<uint8_t> &frame);

    // For APS encryption if needed
    bool apsEncrypt(const std::vector<uint8_t> &plaintext,
                    uint64_t extAddr, // device link key
                    std::vector<uint8_t> &ciphertext);
    bool apsDecrypt(std::vector<uint8_t> &frame, uint64_t extAddr);

    // Key management
    void setNetworkKey(const std::vector<uint8_t> &nwkKey);
    void setLinkKey(uint64_t extAddr, const std::vector<uint8_t> &key);

private:
    std::mutex m_mutex;
    std::vector<uint8_t> m_nwkKey;
    uint32_t m_nwkTxFrameCounter;
    // map extAddr -> linkKey
    std::unordered_map<uint64_t, std::vector<uint8_t>> m_linkKeys;
};
