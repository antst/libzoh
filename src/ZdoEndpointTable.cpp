#include "ZdoEndpointTable.h"

void ZdoEndpointTable::addSimpleDescriptor(const SimpleDescriptor &desc) {
    m_epMap[desc.endpoint] = desc;
}

bool ZdoEndpointTable::getSimpleDescriptor(uint8_t endpoint,
                                           SimpleDescriptor &outDesc) const {
    auto it = m_epMap.find(endpoint);
    if (it == m_epMap.end()) return false;
    outDesc = it->second;
    return true;
}

std::vector<uint8_t> ZdoEndpointTable::getActiveEndpoints() const {
    std::vector<uint8_t> eps;
    for (auto &kv: m_epMap)
        eps.push_back(kv.first);
    return eps;
}
