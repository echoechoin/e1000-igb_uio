#ifndef _ARP_H_
#define _ARP_H_

#include <stdint.h>

#define ARP_HW_TYPE_ETHERNET 0x0001
#define ARP_OP_REQUEST 0x0001

struct arp_hdr {
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t hw_addr_len;
    uint8_t proto_addr_len;
    uint16_t opcode;
    uint8_t sender_hw_addr[6];
    uint32_t sender_ip_addr;
    uint8_t target_hw_addr[6];
    uint32_t target_ip_addr;
} __attribute__((packed));

#endif
