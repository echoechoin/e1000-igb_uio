#ifndef _ETHERNET_H_
#define _ETHERNET_H_

#include <stdint.h>

#define ETH_TYPE_ARP 0x0806
#define ETH_TYPE_IP 0x0800

struct eth_hdr {
    uint8_t dst[6];
    uint8_t src[6];
    uint16_t type;
} __attribute__((packed));

#endif
