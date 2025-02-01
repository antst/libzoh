libzoh 
========= 

### Note:
This is an early sketch and do not pretend to be even remotely close to working solution!

===

This is project of zigbee-on-host library, which implements host-based zigbee stack, 
which communicates with radio co-processor (RCP) via Spinel protocol via UART.
Aim is to provide stack, which is compatible with broad range of zigbee chips, 
due to minimalistic requirement for RCP firmware to support Raw Spinel protocol frames.  
This stack is supposed to be used either for implementation of host-based coordinator,
or for host-based implementation of end devices and routers.



