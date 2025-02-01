#include "../include/SecurityManager.h"
#include <iostream>

SecurityManager::SecurityManager()
        : m_nwkTxFrameCounter(1) {
}

SecurityManager::~SecurityManager() {
}

bool SecurityManager::nwkEncrypt(const std::vector<uint8_t> &plaintext,
                                 uint32_t &txFrameCounter,
                                 std::vector<uint8_t> &ciphertext) {
    std::lock_guard<std::mutex> lock(m_mutex);
    txFrameCounter = m_nwkTxFrameCounter++;
    // Here do real AES-CCM
    // For stub, we just copy
    ciphertext = plaintext;
    return true;
}

bool SecurityManager::nwkDecrypt(std::vector<uint8_t> &frame) {
    std::lock_guard<std::mutex> lock(m_mutex);
    // do AES-CCM
    // stub = do nothing
    return true;
}

bool SecurityManager::apsEncrypt(const std::vector<uint8_t> &plaintext,
                                 uint64_t extAddr,
                                 std::vector<uint8_t> &ciphertext) {
    std::lock_guard<std::mutex> lock(m_mutex);
    // find linkKey in m_linkKeys
    auto it = m_linkKeys.find(extAddr);
    if (it == m_linkKeys.end()) return false;
    // do AES-CCM with that key
    ciphertext = plaintext; // stub
    return true;
}

bool SecurityManager::apsDecrypt(std::vector<uint8_t> &frame, uint64_t extAddr) {
    std::lock_guard<std::mutex> lock(m_mutex);
    // stub
    return true;
}

void SecurityManager::setNetworkKey(const std::vector<uint8_t> &nwkKey) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_nwkKey = nwkKey;
    m_nwkTxFrameCounter = 1;
}

void SecurityManager::setLinkKey(uint64_t extAddr, const std::vector<uint8_t> &key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_linkKeys[extAddr] = key;
}
