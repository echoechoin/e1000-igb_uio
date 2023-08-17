#include <stdio.h>
#include "e1000.h"
#include "ethernet.h"
#include "assert.h"

int main()
{
    struct e1000_device *dev = e1000_device_get("0000:02:02.0");
    if (!dev) {
        printf("e1000_device_get failed\n");
        return -1;
    }
    e1000_init(dev);
    printf("MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
           dev->mac_addr[0], dev->mac_addr[1],
           dev->mac_addr[2], dev->mac_addr[3],
           dev->mac_addr[4], dev->mac_addr[5]);

    // 轮询接收描述符
    printf("start polling\n");
    struct rx_desc_t *desc;
    struct rx_desc_t *virt = phys_to_virt(dev->rx_desc);
    int count = 0;
    while (1) {
        desc = &virt[dev->rx_cur];
        if ((desc->status & RS_DD) == 0)
            continue;
        if (desc->error) {
            printf("receive error\n");
            continue;
        }

        assert(desc->length < 2048);
        struct eth_hdr *hdr = phys_to_virt((void *)desc->addr);
        printf("receive desc:        %d\n", dev->rx_cur);
        printf("receive total:       %d\n", ++count);
        printf("receive packet src:  %02x:%02x:%02x:%02x:%02x:%02x\n",
                hdr->src[0], hdr->src[1], hdr->src[2],
                hdr->src[3], hdr->src[4], hdr->src[5]);
        printf("               dest: %02x:%02x:%02x:%02x:%02x:%02x\n",
                hdr->dst[0], hdr->dst[1], hdr->dst[2],
                hdr->dst[3], hdr->dst[4], hdr->dst[5]);
        
        desc->status = 0;

        E1000_WRITE_REG(dev->hw_addr, E1000_RDT, dev->rx_cur);
        dev->rx_cur = (dev->rx_cur + 1) % RX_DESC_NR;
    }
    return 0;
}