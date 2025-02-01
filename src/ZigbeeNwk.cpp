#include "../include/ZigbeeNwk.h"
#include <iostream>

ZigbeeNwk::ZigbeeNwk(ZigbeeMac &mac, SecurityManager &sec)
        : m_mac(mac), m_sec(sec), m_sequenceNumber(0), m_radius(10), m_running(false) {
}

ZigbeeNwk::~ZigbeeNwk() {
    stop();
}

void ZigbeeNwk::start() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_running) {
        m_running = true;
        // set mac callbacks
        m_mac.setMacFrameHandler(
                [this](const std::vector<uint8_t> &frame, int8_t rssi, uint8_t lqi) {
                    handleInboundMacFrame(frame, rssi, lqi);
                }
        );
        m_mac.setMacTxDoneHandler(
                [this](bool success) { handleMacTxDone(success); }
        );
        m_maintenanceThread = std::thread(&ZigbeeNwk::maintenanceThreadLoop, this);

        m_mac.start();
    }
}

void ZigbeeNwk::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_running) {
        m_running = false;
        m_mac.setMacFrameHandler(nullptr);
        m_mac.setMacTxDoneHandler(nullptr);
        if (m_maintenanceThread.joinable()) {
            m_maintenanceThread.join();
        }
        m_mac.stop();
    }
}

void ZigbeeNwk::setNwkPayloadCallback(NwkPayloadHandler cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cb = cb;
}

bool ZigbeeNwk::sendNwkFrame(const std::vector<uint8_t> &payload, uint16_t dstAddr, uint8_t radius) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_running) return false;
    if (m_ownShortAddr == 0xFFFF) {
        std::cerr << "[NWK] Error: not joined or not coordinator.\n";
        return false;
    }


    // 1) Determine route
    uint16_t nextHop = dstAddr;
    bool useSrcRoute = false;
    if (dstAddr != 0xFFFF) // not broadcast
    {
        if (!lookupRoute(dstAddr, nextHop, useSrcRoute)) {
            // route not found, initiate route discovery
            bool ok = initiateRouteDiscovery(dstAddr);
            if (!ok) {
                std::cerr << "[NWK] Route discovery failed to start.\n";
                return false;
            }
            // We'll queue or drop this frame for now; real code might queue.
            std::cerr << "[NWK] No route yet, discarding frame or buffering.\n";
            return false;
        }
    }

    // If we have a route entry with sourceRoute, pass it in
    std::vector<uint16_t> routePath;
    if (useSrcRoute) {
        auto it = m_routeTable.find(dstAddr);
        if (it != m_routeTable.end() && !it->second.sourceRoute.empty()) {
            routePath = it->second.sourceRoute;
        }
    }

    // 2) Build NWK frame
    bool security = false;
    bool srcRouteFlag = !routePath.empty();
    if (radius == 0) radius = m_radius;
    std::vector<uint8_t> nwkFrame = buildNwkFrame(payload, dstAddr, m_ownShortAddr, radius, m_sequenceNumber++,
                                                  security, srcRouteFlag, routePath);
    // NWK encrypt
    if (!nwkEncrypt(nwkFrame))
        return false;
    // 3) Send via MAC
    //FIXME: Form MAC frame
    std::vector<uint8_t> macFrame;//= buildMacFrame(nwkFrame, m_ownShortAddr, nextHop, m_panId);
    return m_mac.sendFrame(macFrame);
}


void ZigbeeNwk::handleMacTxDone(bool success) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!success) {
        std::cout << "[NWK] MAC TX fail => route repair?\n";
        // route repair logic if needed
    }
}

bool ZigbeeNwk::nwkEncrypt(std::vector<uint8_t> &macFrame) {
    // call security manager
    uint32_t frameCounter = 0;
    std::vector<uint8_t> cipher;
    bool ok = m_sec.nwkEncrypt(macFrame, frameCounter, cipher);
    if (!ok) return false;
    macFrame = cipher;
    return true;
}

bool ZigbeeNwk::nwkDecrypt(std::vector<uint8_t> &macFrame) {
    return m_sec.nwkDecrypt(macFrame);
}

/**
 * This thread runs periodically, broadcasting link status and pruning neighbors.
 */
void ZigbeeNwk::maintenanceThreadLoop() {
    using namespace std::chrono_literals;
    while (m_running) {
        // Sleep 15s
        std::this_thread::sleep_for(15s);

        // Send link status
        sendLinkStatus();

        // Prune neighbors
        pruneNeighbors();
    }
}


void ZigbeeNwk::sendLinkStatus() {
    // Build a NWK command with cmdId=0x06, containing neighbor count + LQI data
    // For brevity, we only include the count of neighbors

    uint8_t cmdId = 0x06;
    std::vector<uint8_t> payload;
    payload.push_back(cmdId);

    // E.g., push back neighbor count
    uint8_t nbrCount = (uint8_t) m_neighborTable.size();
    payload.push_back(nbrCount);

    // Possibly for each neighbor, push shortAddr, linkQuality, etc.
    for (auto &kv: m_neighborTable) {
        uint16_t nbr = kv.first;
        payload.push_back(nbr & 0xFF);
        payload.push_back((nbr >> 8) & 0xFF);
        payload.push_back(kv.second.linkQuality);
    }

    // Broadcast NWK
    // We can set a typical radius of e.g. 1 or 2 since link status is usually local
    // but for demonstration, let's do radius=2
    sendNwkFrame(payload, 0xFFFF /* broadcast */, 2);
    std::cout << "[NWK] Sent link status, neighbors=" << (int) nbrCount << "\n";
}

void ZigbeeNwk::handleLinkStatusFrame(const NwkFrameHeader &hdr,
                                      const uint8_t *payload,
                                      size_t length) {
    if (length < 2) return;
    uint8_t nbrCount = payload[1];
    size_t need = 2 + nbrCount * 3; // each neighbor is shortAddr(2) + lqi(1)
    if (length < need) return;

    std::cout << "[NWK] Received link status from 0x"
              << std::hex << hdr.sourceAddr
              << ", neighborCount=" << std::dec << (int) nbrCount << "\n";

    // Could update local link costs or track which neighbors the sender sees, etc.
    // We'll just parse and ignore
}

void ZigbeeNwk::pruneNeighbors() {
    using namespace std::chrono;
    auto now = steady_clock::now();
    const auto STALE_TIMEOUT = 300s; // 5 minutes

    // We'll gather to-remove in a vector so we don't erase in loop
    std::vector<uint16_t> toRemove;
    for (auto &kv: m_neighborTable) {
        auto &entry = kv.second;
        if ((now - entry.lastHeard) > STALE_TIMEOUT) {
            toRemove.push_back(kv.first);
        }
    }
    for (auto addr: toRemove) {
        m_neighborTable.erase(addr);
        std::cout << "[NWK] Pruned stale neighbor 0x"
                  << std::hex << addr << "\n";
    }
}

void ZigbeeNwk::sendRouteError(uint16_t originAddr, uint16_t brokenAddr) {
    // NWK command [ cmdId=0x05, origin(2B), broken(2B) ]
    std::vector<uint8_t> payload;
    payload.push_back(0x05); // route error cmd
    payload.push_back(originAddr & 0xFF);
    payload.push_back((originAddr >> 8) & 0xFF);
    payload.push_back(brokenAddr & 0xFF);
    payload.push_back((brokenAddr >> 8) & 0xFF);

    // Usually unicast to origin. If we have a route to the origin, we use it
    sendNwkFrame(payload, originAddr);
    std::cout << "[NWK] Sent route error to origin=0x"
              << std::hex << originAddr
              << " for broken=0x" << brokenAddr << "\n";
}

void ZigbeeNwk::handleRouteError(const NwkFrameHeader &hdr,
                                 const uint8_t *payload,
                                 size_t length) {
    // [cmdId=0x05, origin(2B), broken(2B)]
    if (length < 5) return;
    uint16_t origin = payload[1] | (payload[2] << 8);
    uint16_t broken = payload[3] | (payload[4] << 8);

    std::cout << "[NWK] RouteError from 0x" << std::hex << hdr.sourceAddr
              << ": origin=0x" << origin
              << ", broken=0x" << broken << "\n";

    // If I'm the origin, I might mark that route as FAILED or do route repair
    if (origin == m_ownShortAddr) {
        auto it = m_routeTable.find(broken);
        if (it != m_routeTable.end()) {
            it->second.state = RouteState::FAILED;
            std::cout << "[NWK] Mark route to 0x" << std::hex << broken
                      << " as FAILED due to route error.\n";
            repairRoute(broken);
        }
    }
}

/********************************************************************
 * Route Repair
 ********************************************************************/
bool ZigbeeNwk::repairRoute(uint16_t dstAddr) {
    // Mark route as REPAIR_IN_PROGRESS, send route error or do local route discovery
    auto it = m_routeTable.find(dstAddr);
    if (it == m_routeTable.end())
        return false;
    it->second.state = RouteState::REPAIR_IN_PROGRESS;

    // we do a new route discovery
    return initiateRouteDiscovery(dstAddr);
}


void ZigbeeNwk::updateNeighbor(uint16_t shortAddr, uint64_t extAddr, int8_t rssi, uint8_t lqi) {
    auto now = std::chrono::steady_clock::now();
    auto &nbr = m_neighborTable[shortAddr];
    nbr.shortAddr = shortAddr;
    if (extAddr != 0) nbr.extAddr = extAddr;
    nbr.lastRssi = rssi;
    nbr.linkQuality = lqi;
    nbr.lastHeard = now;
}

/********************************************************************
 * handleInboundMacFrame
 *   - parse NWK header
 *   - handle route commands
 *   - handle MTO or source route
 ********************************************************************/
void ZigbeeNwk::handleInboundMacFrame(const std::vector<uint8_t> &macFrame,
                                      int8_t rssi, uint8_t lqi) {
    std::vector<uint8_t> macPayload = macFrame;
    if (!nwkDecrypt(macPayload))
        return;
    // parse NWK header
    NwkFrameHeader hdr;
    size_t headerLen = 0;
    std::vector<uint16_t> srcRoute;
    if (!parseNwkHeader(macPayload, hdr, headerLen, srcRoute)) {
        std::cerr << "[NWK] parse NWK header failed\n";
        return;
    }

    // update neighbor
    updateNeighbor(hdr.sourceAddr, 0, rssi, lqi); // extAddr=0 for demo

    // check if NWK payload is a route command
    if (headerLen < macPayload.size()) {
        const uint8_t *payloadPtr = macPayload.data() + headerLen;
        size_t payloadLen = macPayload.size() - headerLen;

        // check command ID
        if (payloadLen > 0) {
            auto cmd = static_cast<NwkCommand>(payloadPtr[0]);
            switch (cmd) {
                case NwkCommand::ROUTE_REQUEST: // normal route request
                    handleRouteRequest(hdr, payloadPtr, payloadLen);
                    return;
                case NwkCommand::ROUTE_REPLY: // route reply
                    handleRouteReply(hdr, payloadPtr, payloadLen);
                    return;
                case NwkCommand::MTO_ROUTE_REQUEST: // MTO route request
                    handleManyToOneRequest(hdr, payloadPtr, payloadLen);
                    return;
                case NwkCommand::ROUTE_RECORD: // route record command
                    handleRouteRecord(hdr, payloadPtr, payloadLen);
                    return;
                case NwkCommand::ROUTE_ERROR: // route error
                    handleRouteError(hdr, payloadPtr, payloadLen);
                    return;
                case NwkCommand::LINK_STATUS: // link status
                    handleLinkStatusFrame(hdr, payloadPtr, payloadLen);
                    return;
                case NwkCommand::DATA:
                default:
                    // If the command is DATA (0x00) or unrecognized, pass it upward.
                    std::cout << "[ZigbeeNwk] Passing NWK data to upper layer (no command processing).\n";
                    break;
            }
        }
    }

    // if not a route command
    // if we're the final dest or broadcast
    if (hdr.destinationAddr == m_ownShortAddr || hdr.destinationAddr == 0xFFFF) {
        // deliver up
        if (m_cb && headerLen < macPayload.size()) {
            std::vector<uint8_t> nwkPayload(macPayload.begin() + headerLen, macPayload.end());
            m_cb(nwkPayload, hdr.sourceAddr, hdr.destinationAddr);
        }
    } else {
        // forward using next hop or source route stepping
        if (!srcRoute.empty()) {
            // If there's a source route, find the next hop index
            // We'll do a naive approach: assume the first entry is the origin, the last is final
            // We find the current node in the route list, then pick the next node
            auto it = std::find(srcRoute.begin(), srcRoute.end(), m_ownShortAddr);
            if (it != srcRoute.end() && (it + 1) != srcRoute.end()) {
                uint16_t nextHop = *(it + 1);
                // forward
                sendNwkFrame(macPayload, nextHop);
            }
        } else {
            // normal routing
            uint16_t nextHop = 0;
            bool useSrcRoute = false;
            if (lookupRoute(hdr.destinationAddr, nextHop, useSrcRoute)) {
                sendNwkFrame(macPayload, nextHop);
            } else {
                // route error or attempt repair
                repairRoute(hdr.destinationAddr);
            }
        }
    }
}

/********************************************************************
 * Many-to-One route discovery
 ********************************************************************/
bool ZigbeeNwk::initiateManyToOneDiscovery(uint8_t radius) {
    // Usually done by the coordinator to gather routes from all nodes back to coordinator
    // We send a "MTO route request" broadcast
    return sendMtoRouteRequest(radius);
}

bool ZigbeeNwk::sendMtoRouteRequest(uint8_t radius) {
    // NWK payload = [cmdId=0x03, radius]
    std::vector<uint8_t> payload;
    payload.push_back(0x03); // MTO request command
    payload.push_back(radius);

    // broadcast
    return sendNwkFrame(payload, 0xFFFF);
}

void ZigbeeNwk::handleManyToOneRequest(const NwkFrameHeader &nwkHdr,
                                       const uint8_t *payload, size_t length) {
    // MTO request means "create a route back to the sender"
    // For example, we store nextHop = the neighbor from which we got this
    // Then we can route to aggregator
    // If we see radius>1, we forward broadcast
    if (length < 2) return;
    uint8_t radius = payload[1];

    // create route table entry to NWK source = aggregator
    setRoute(nwkHdr.sourceAddr, nwkHdr.sourceAddr, {}, true);

    if (nwkHdr.radius > 1) {
        // forward
        // naive re-broadcast
        std::vector<uint8_t> fwd(payload, payload + length);
        sendNwkFrame(fwd, 0xFFFF);
    }
}

/********************************************************************
 * Source Routing: route record command
 ********************************************************************/
void ZigbeeNwk::handleRouteRecord(const NwkFrameHeader &nwkHdr,
                                  const uint8_t *payload, size_t length) {
    // route record might contain [cmdId=0x04, count(1B), list of shortAddrs]
    if (length < 2) return;
    uint8_t count = payload[1];
    if (length < 2 + count * 2) return;

    std::vector<uint16_t> path;
    const uint8_t *p = payload + 2;
    for (int i = 0; i < count; i++) {
        uint16_t hop = p[0] | (p[1] << 8);
        p += 2;
        path.push_back(hop);
    }

    // The last entry might be the NWKHdr's source
    // store as a source route in route table
    setRoute(nwkHdr.sourceAddr, nwkHdr.sourceAddr, path, false);
}

/********************************************************************
 * Initiate normal route discovery
 ********************************************************************/
bool ZigbeeNwk::initiateRouteDiscovery(uint16_t dst) {
    auto &entry = m_routeTable[dst];
    if (entry.state == RouteState::DISCOVERY_IN_PROGRESS ||
        entry.state == RouteState::REPAIR_IN_PROGRESS) {
        // already in progress
        return true;
    }

    entry.state = RouteState::DISCOVERY_IN_PROGRESS;
    entry.discoverySeq++;

    // NWK route request (cmdId=0x01)
    std::vector<uint8_t> rr;
    rr.push_back(0x01); // route request
    rr.push_back(entry.discoverySeq);
    rr.push_back(dst & 0xFF);
    rr.push_back((dst >> 8) & 0xFF);

    // broadcast
    return sendNwkFrame(rr, 0xFFFF);
}


/********************************************************************
 * Route Request / Reply (unchanged from simpler code)
 ********************************************************************/
void ZigbeeNwk::handleRouteRequest(const NwkFrameHeader &nwkHdr,
                                   const uint8_t *payload, size_t length) {
    // parse discSeq, target, forward, or if we're target -> route reply
}

void ZigbeeNwk::handleRouteReply(const NwkFrameHeader &nwkHdr,
                                 const uint8_t *payload, size_t length) {
    // parse route reply, set route to origin or final
}

/********************************************************************
* setRoute
********************************************************************/
void ZigbeeNwk::setRoute(uint16_t dst, uint16_t nextHop, const std::vector<uint16_t> &sourceRoute, bool mto) {
    auto &r = m_routeTable[dst];
    r.destination = dst;
    r.nextHop = nextHop;
    r.state = RouteState::ACTIVE;
    r.sourceRoute = sourceRoute;
    r.manyToOne = mto;
}

/********************************************************************
 * lookupRoute
 ********************************************************************/
bool ZigbeeNwk::lookupRoute(uint16_t dst, uint16_t &outNextHop, bool &outUseSourceRoute) {
    auto it = m_routeTable.find(dst);
    if (it == m_routeTable.end()) return false;
    auto &r = it->second;
    if (r.state != RouteState::ACTIVE) return false;
    // if we have a sourceRoute stored, set outUseSourceRoute
    outUseSourceRoute = !r.sourceRoute.empty();
    outNextHop = r.nextHop;
    return true;
}

/********************************************************************
 * Debug Helpers
 ********************************************************************/
std::string ZigbeeNwk::debugRouteTable() const {
    std::ostringstream oss;
    oss << "[NWK RouteTable]\n";
    for (auto &kv: m_routeTable) {
        auto &rt = kv.second;
        oss << "  dst=0x" << std::hex << rt.destination
            << " nextHop=0x" << rt.nextHop
            << " state=" << (int) rt.state
            << " MTO=" << rt.manyToOne
            << " srcRouteCount=" << rt.sourceRoute.size() << "\n";
    }
    return oss.str();
}

std::string ZigbeeNwk::debugNeighborTable() const {
    std::ostringstream oss;
    oss << "[NWK NeighborTable]\n";
    for (auto &kv: m_neighborTable) {
        auto &n = kv.second;
        oss << "  short=0x" << std::hex << n.shortAddr
            << " LQI=" << std::dec << (int) n.linkQuality
            << " RSSI=" << (int) n.lastRssi << "\n";
    }
    return oss.str();
}


bool ZigbeeNwk::parseNwkHeader(const std::vector<uint8_t> &data, NwkFrameHeader &hdr, size_t &headerLen,
                               std::vector<uint16_t> &parsedSourceRoute) {
    if (data.size() < 7) return false;
    uint8_t fc = data[0];
    hdr.frameControl.security = (fc & 0x01) != 0;
    hdr.frameControl.sourceRoute = (fc & 0x08) != 0;
    hdr.sequenceNumber = data[1];
    hdr.destinationAddr = data[2] | (data[3] << 8);
    hdr.sourceAddr = data[4] | (data[5] << 8);
    hdr.radius = data[6];
    headerLen = 7;

    parsedSourceRoute.clear();
    if (hdr.frameControl.sourceRoute) {
        // check if there's a route record extension
        if (data.size() > headerLen + 1 && data[headerLen] == 0x04) {
            // next byte is count
            if (data.size() < headerLen + 2) return false;
            uint8_t count = data[headerLen + 1];
            size_t needed = headerLen + 2 + count * 2;
            if (data.size() < needed) return false;

            size_t offset = headerLen + 2;
            for (int i = 0; i < count; i++) {
                uint16_t hop = data[offset] | (data[offset + 1] << 8);
                offset += 2;
                parsedSourceRoute.push_back(hop);
            }
            headerLen = offset;
        }
    }
    return true;
}


/********************************************************************
 * Build/Parse NWK with Source Route extension
 ********************************************************************/
std::vector<uint8_t> ZigbeeNwk::buildNwkFrame(const std::vector<uint8_t> &nwkPayload,
                                              uint16_t dstAddr, uint16_t srcAddr,
                                              uint8_t radius, uint8_t seqNum,
                                              bool security, bool sourceRoute,
                                              const std::vector<uint16_t> &srcRoute) {
    // NWK header: [frameControl(1B), seq(1B), dstAddr(2B), srcAddr(2B), radius(1B), (source route extension?), payload...]
    std::vector<uint8_t> result;
    uint8_t fc = 0;
    if (security) fc |= 0x01;
    if (sourceRoute) fc |= 0x08; // example bit for source route
    result.push_back(fc);
    result.push_back(seqNum);
    result.push_back(dstAddr & 0xFF);
    result.push_back((dstAddr >> 8) & 0xFF);
    result.push_back(srcAddr & 0xFF);
    result.push_back((srcAddr >> 8) & 0xFF);
    result.push_back(radius);

    if (sourceRoute && !srcRoute.empty()) {
        // insert some extension for source route, e.g. [count(1B), list of hops(2B each)]
        uint8_t count = (uint8_t) srcRoute.size();
        result.push_back(0x04); // let's say 0x04 indicates route record extension
        result.push_back(count);
        for (auto hop: srcRoute) {
            result.push_back(hop & 0xFF);
            result.push_back((hop >> 8) & 0xFF);
        }
    }

    // then NWK payload
    result.insert(result.end(), nwkPayload.begin(), nwkPayload.end());
    return result;
}