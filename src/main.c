#include <stdio.h>
#include <unistd.h>
#include "e1000.h"
#include "ethernet.h"
#include "assert.h"
#include "arpa/inet.h"
#include "ethernet.h"
#include "arp.h"

#define RECV_MODE 0
#define SEND_MODE 1

static char PCI_ID[PCI_PRI_STR_SIZE + 1] = {0};
static int MODE = RECV_MODE;

static void usage()
{
    printf("usage: ./e1000_test -i <pci_id> -m <recv|send>\n");
    exit(0);
}

static int parse_args(int argc, char **argv)
{
    int opt;
    while ((opt = getopt(argc, argv, "i:m:h")) != -1) {
        switch (opt) {
        case 'h':
            usage();
        case 'i':
            snprintf(PCI_ID, PCI_PRI_STR_SIZE, "%s", optarg);
            break;
        case 'm':
            if (strcmp(optarg, "recv") == 0) {
                MODE = RECV_MODE;
            } else if (strcmp(optarg, "send") == 0) {
                MODE = SEND_MODE;
            } else {
                printf("invalid mode\n");
                return -1;
            }
            break;
        default:
            printf("invalid args\n");
            return -1;
        }
    }
    if (PCI_ID[0] == '\0') {
        printf("please specify pci id\n");
        return -1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    if (parse_args(argc, argv) < 0) {
        usage();
        return -1;
    }

    struct e1000_device *dev = e1000_device_get(PCI_ID);
    if (!dev) {
        printf("e1000_device_get failed\n");
        return -1;
    }

    e1000_init(dev);
    printf("get NIC MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
           dev->mac_addr[0], dev->mac_addr[1],
           dev->mac_addr[2], dev->mac_addr[3],
           dev->mac_addr[4], dev->mac_addr[5]);

    if (MODE == RECV_MODE) {
        printf("start recv...\n");
        char *buf = malloc(2048);
        while (1) {
            e1000_recv(dev, buf, 2048);
            struct eth_hdr *hdr = (struct eth_hdr *)buf;
            printf("receive packet src:  %02x:%02x:%02x:%02x:%02x:%02x\n",
                    hdr->src[0], hdr->src[1], hdr->src[2],
                    hdr->src[3], hdr->src[4], hdr->src[5]);
            printf("               dest: %02x:%02x:%02x:%02x:%02x:%02x\n",
                    hdr->dst[0], hdr->dst[1], hdr->dst[2],
                    hdr->dst[3], hdr->dst[4], hdr->dst[5]); 
        }
    } else {
        // send arp gratuitous per 1s
        printf("sending arp gratuitous\n");
        char *buf = malloc(2048);
        struct eth_hdr *hdr = (struct eth_hdr *)buf;
        for (int i = 0; i < 6; i++) {
            hdr->dst[i] = 0xff;
            hdr->src[i] = dev->mac_addr[i];
        }
        hdr->type = htons(ETH_TYPE_ARP);
        struct arp_hdr *arp = (struct arp_hdr *)(buf + sizeof(struct eth_hdr));

        arp->hw_type = htons(ARP_HW_TYPE_ETHERNET);
        arp->proto_type = htons(ETH_TYPE_IP);
        arp->hw_addr_len = 6;
        arp->proto_addr_len = 4;
        arp->opcode = htons(ARP_OP_REQUEST);
        for (int i = 0; i < 6; i++) {
            arp->sender_hw_addr[i] = dev->mac_addr[i];
            arp->target_hw_addr[i] = 0xff;
        }

        arp->sender_ip_addr = inet_addr("1.2.3.4");
        arp->target_ip_addr = inet_addr("1.2.3.4");
        int count = 0;
        while (1) {
            printf("send arp gratuitous %d\n", count++);
            e1000_send(dev, buf, sizeof(struct eth_hdr) + sizeof(struct arp_hdr));
            sleep(1);
        }
    }
    return 0;
}