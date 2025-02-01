#include <iostream>
#include "../../include/platform/RawSpinelDriver.h"
#include "../../include/ZigbeeMac.h"
#include "../../include/ZigbeeNwk.h"
#include "../../include/ZigbeeAps.h"
#include "../../include/ZigbeeZdo.h"
#include "../../include/SecurityManager.h"

int main()
{
    // 1) create spinel driver
    RawSpinelDriver spinel("/dev/ttyUSB0", 115200);

    // 2) create security manager
    SecurityManager secMgr;
    // set a NWK key
    secMgr.setNetworkKey({0xAA,0xBB,0xCC,0xDD,0xEE,0xFF,0x11,0x22,
                          0x33,0x44,0x55,0x66,0x77,0x88,0x99,0x00});

    // 3) create MAC
    ZigbeeMac mac(spinel);

    // 4) create NWK
    ZigbeeNwk nwk(mac, secMgr);

    // 5) create APS
    ZigbeeAps aps(nwk, secMgr);

    // 6) create ZDO
    ZigbeeZdo zdo(aps,0xFFFF, 0x000000000000FFFF);

    // Start from bottom up
    zdo.start(); // internally starts APS -> NWK -> MAC
    mac.enableMacAssociation(true);

    // do some config
    mac.setChannel(15);
    mac.setPanId(0x1234);

    // let it run
    while(true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    return 0;
}
