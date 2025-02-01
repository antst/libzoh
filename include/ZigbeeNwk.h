#pragma once

#include <cstdint>
#include <vector>
#include <mutex>
#include "ZigbeeMac.h"
#include "SecurityManager.h"

using NwkFrameIndicationCallback = std::function<void(const std::vector<uint8_t> &nwkPayload,
                                                      uint16_t srcAddr,
                                                      uint16_t dstAddr)>;

enum class RouteState {
    IDLE = 0,
    DISCOVERY_IN_PROGRESS,
    ACTIVE,
    REPAIR_IN_PROGRESS,
    FAILED
};

struct NeighborTableEntry {
    uint16_t shortAddr;
    uint64_t extAddr;
    int8_t lastRssi;
    uint8_t linkQuality;
    std::chrono::steady_clock::time_point lastHeard;
};

struct RouteTableEntry {
    uint16_t destination;
    uint16_t nextHop;
    RouteState state;
    uint8_t discoverySeq;
    bool manyToOne;
    std::vector<uint16_t> sourceRoute;
};

struct NwkFrameControl {
    bool security = false;
    bool discover = false;
    bool multicast = false;
    bool sourceRoute = false;
};

struct NwkHeader {
    NwkFrameControl frameControl;
    uint8_t sequenceNumber;
    uint16_t sourceAddr;
    uint16_t destinationAddr;
    uint8_t radius;
};


class ZigbeeNwk {
public:
    ZigbeeNwk(ZigbeeMac &mac, SecurityManager &sec);

    ~ZigbeeNwk();

    void start();

    void stop();

    void setFrameIndicationCallback(NwkFrameIndicationCallback cb);

    // Outbound NWK data
    bool sendNwkFrame(const std::vector<uint8_t> &payload, uint16_t dstAddr, uint8_t radius);

    /**
    * @brief Called by MAC layer when a raw MAC frame arrives that is identified as NWK.
    *        We parse the NWK header, handle route, and possibly deliver up or forward.
    */
    void handleInboundMacFrame(const std::vector<uint8_t> &macPayload,
                               int8_t rssi, uint8_t lqi);

    // Route/repair
    bool repairRoute(uint16_t dstAddr);

    bool initiateManyToOneDiscovery(uint8_t radius);

    // Additional expansions:
    void startMaintenance(); // launches a background thread for link status, neighbor pruning
    void stopMaintenance();

    std::string debugRouteTable() const;

    std::string debugNeighborTable() const;

private:
    // Inbound from MAC
    void handleMacFrame(const std::vector<uint8_t> &macFrame);

    // TX done from MAC
    void handleMacTxDone(bool success);

    // NWK encrypt / decrypt
    bool nwkEncrypt(std::vector<uint8_t> &macFrame);

    bool nwkDecrypt(std::vector<uint8_t> &macFrame);

    // Internal
    void maintenanceThreadLoop();

    void sendLinkStatus();

    void handleLinkStatusFrame(const NwkHeader &hdr, const uint8_t *payload, size_t length);

    void pruneNeighbors();

    void sendRouteError(uint16_t originAddr, uint16_t brokenAddr);

    // Helper for neighbor table
    void updateNeighbor(uint16_t shortAddr, uint64_t extAddr, int8_t rssi, uint8_t lqi);

    // Route table ops
    bool lookupRoute(uint16_t dst, uint16_t &outNextHop, bool &outUseSourceRoute);

    void setRoute(uint16_t dst, uint16_t nextHop, const std::vector<uint16_t> &sourceRoute, bool mto = false);

    // Initiate normal route discovery
    bool initiateRouteDiscovery(uint16_t dst);

    // NWK frame build/parse
    std::vector<uint8_t> buildNwkFrame(const std::vector<uint8_t> &nwkPayload,
                                       uint16_t dstAddr, uint16_t srcAddr,
                                       uint8_t radius, uint8_t seqNum,
                                       bool security, bool sourceRoute,
                                       const std::vector<uint16_t> &srcRoute);

    bool parseNwkHeader(const std::vector<uint8_t> &data, NwkHeader &hdr, size_t &headerLen,
                        std::vector<uint16_t> &parsedSourceRoute);

    // Handlers for route commands
    void handleRouteRequest(const NwkHeader &nwkHdr, const uint8_t *payload, size_t length);

    void handleRouteReply(const NwkHeader &nwkHdr, const uint8_t *payload, size_t length);

    void handleManyToOneRequest(const NwkHeader &nwkHdr, const uint8_t *payload, size_t length);

    void handleRouteRecord(const NwkHeader &nwkHdr, const uint8_t *payload, size_t length);

    // MTO route request
    bool sendMtoRouteRequest(uint8_t radius);

    void handleRouteError(const NwkHeader &hdr, const uint8_t *payload, size_t length);

    ZigbeeMac &m_mac;
    SecurityManager &m_sec;

    std::mutex m_mutex;
    NwkFrameIndicationCallback m_cb;

    // neighbor/route tables
    std::unordered_map<uint16_t, NeighborTableEntry> m_neighborTable;
    std::unordered_map<uint16_t, RouteTableEntry> m_routeTable;
    uint8_t m_sequenceNumber;
    uint16_t m_ownShortAddr;
    uint16_t m_panId;
    uint8_t m_channel;
    uint8_t m_radius;

    // For the maintenance thread
    std::thread m_maintenanceThread;
    std::atomic<bool> m_running;
};
