#ifndef _E1000_H_
#define _E1000_H_
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include "e1000.h"
#include "mem_alloc.h"

#define PCI_PRI_STR_SIZE sizeof("XXXXXXXX:XX:XX.X")

// 寄存器偏移
enum REGISTERS
{
    E1000_CTRL = 0x00,   // Device Control 设备控制
    E1000_STATUS = 0x08, // Device Status 设备状态
    E1000_EERD = 0x14,   // EEPROM Read EEPROM 读取
    /**
     *
     * +-------+---------+----------+------+----------+-------+
     * | 31-16 | 15 - 8  |   7 - 5  |   4  |  3 - 1   |   0   |
     * +-------+---------+----------+------+----------+-------+
     * | Data  | Address | reserved | done | reserved | start | 
     * +-------+---------+----------+------+----------+-------+
     * start:   向此位写1，开始读取eeprom
     * done:    读取完成后，此位为1
     * Address: 读取的eeprom地址，需要写入
     * Data:    读取的eeprom数据
     */

    E1000_ICR = 0xC0, // Interrupt Cause Read 中断原因读
    E1000_ITR = 0xC4, // Interrupt Throttling 中断节流
    E1000_ICS = 0xC8, // Interrupt Cause Set 中断原因设置
    E1000_IMS = 0xD0, // Interrupt Mask Set/Read 中断掩码设置/读
    E1000_IMC = 0xD8, // Interrupt Mask Clear 中断掩码清除

    E1000_RCTL = 0x100,   // Receive Control 接收控制
    E1000_RDBAL = 0x2800, // Receive Descriptor Base Address LOW 接收描述符低地址
    E1000_RDBAH = 0x2804, // Receive Descriptor Base Address HIGH 64bit only 接收描述符高地址
    E1000_RDLEN = 0x2808, // Receive Descriptor Length 接收描述符长度
    E1000_RDH = 0x2810,   // Receive Descriptor Head 接收描述符头
    E1000_RDT = 0x2818,   // Receive Descriptor Tail 接收描述符尾

    E1000_TCTL = 0x400,   // Transmit Control 发送控制
    E1000_TDBAL = 0x3800, // Transmit Descriptor Base Low 传输描述符低地址
    E1000_TDBAH = 0x3804, // Transmit Descriptor Base High 传输描述符高地址
    E1000_TDLEN = 0x3808, // Transmit Descriptor Length 传输描述符长度
    E1000_TDH = 0x3810,   // TDH Transmit Descriptor Head 传输描述符头
    E1000_TDT = 0x3818,   // TDT Transmit Descriptor Tail 传输描述符尾

    E1000_MAT0 = 0x5200, // Multicast Table Array 05200h-053FCh 组播表数组
    E1000_MAT1 = 0x5400, // Multicast Table Array 05200h-053FCh 组播表数组
};

// 接收控制
enum RCTL
{
    RCTL_EN = 1 << 1,               // Receiver Enable
    RCTL_SBP = 1 << 2,              // Store Bad Packets
    RCTL_UPE = 1 << 3,              // Unicast Promiscuous Enabled
    RCTL_MPE = 1 << 4,              // Multicast Promiscuous Enabled
    RCTL_LPE = 1 << 5,              // Long Packet Reception Enable
    RCTL_LBM_NONE = 0b00 << 6,      // No Loopback
    RCTL_LBM_PHY = 0b11 << 6,       // PHY or external SerDesc loopback
    RTCL_RDMTS_HALF = 0b00 << 8,    // Free Buffer Threshold is 1/2 of RDLEN
    RTCL_RDMTS_QUARTER = 0b01 << 8, // Free Buffer Threshold is 1/4 of RDLEN
    RTCL_RDMTS_EIGHTH = 0b10 << 8,  // Free Buffer Threshold is 1/8 of RDLEN

    RCTL_BAM = 1 << 15, // Broadcast Accept Mode
    RCTL_VFE = 1 << 18, // VLAN Filter Enable

    RCTL_CFIEN = 1 << 19, // Canonical Form Indicator Enable
    RCTL_CFI = 1 << 20,   // Canonical Form Indicator Bit Value
    RCTL_DPF = 1 << 22,   // Discard Pause Frames
    RCTL_PMCF = 1 << 23,  // Pass MAC Control Frames
    RCTL_SECRC = 1 << 26, // Strip Ethernet CRC

    RCTL_BSIZE_256 = 3 << 16,
    RCTL_BSIZE_512 = 2 << 16,
    RCTL_BSIZE_1024 = 1 << 16,
    RCTL_BSIZE_2048 = 0 << 16,
    RCTL_BSIZE_4096 = (3 << 16) | (1 << 25),
    RCTL_BSIZE_8192 = (2 << 16) | (1 << 25),
    RCTL_BSIZE_16384 = (1 << 16) | (1 << 25),
};

// 中断类型
enum IMS
{
    // 传输描述符写回，表示有一个数据包发出
    IM_TXDW = 1 << 0, // Transmit Descriptor Written Back.

    // 传输队列为空
    IM_TXQE = 1 << 1, // Transmit Queue Empty.

    // 连接状态变化，可以认为是网线拔掉或者插上
    IM_LSC = 1 << 2, // Link Status Change

    // 接收序列错误
    IM_RXSEQ = 1 << 3, // Receive Sequence Error.

    // 到达接受描述符最小阈值，表示流量太大，接收描述符太少了，应该再多加一些，不过没有数据包丢失
    IM_RXDMT0 = 1 << 4, // Receive Descriptor Minimum Threshold hit.

    // 因为没有可用的接收缓冲区或因为PCI接收带宽不足，已经溢出，有数据包丢失
    IM_RXO = 1 << 6, // Receiver FIFO Overrun.

    // 接收定时器中断
    IM_RXT0 = 1 << 7, // Receiver Timer Interrupt.

    // 这个位在 MDI/O 访问完成时设置
    IM_MADC = 1 << 9, // MDI/O Access Complete Interrupt

    IM_RXCFG = 1 << 10,  // Receiving /C/ ordered sets.
    IM_PHYINT = 1 << 12, // Sets mask for PHY Interrupt
    IM_GPI0 = 1 << 13,   // General Purpose Interrupts.
    IM_GPI1 = 1 << 14,   // General Purpose Interrupts.

    // 传输描述符环已达到传输描述符控制寄存器中指定的阈值。
    IM_TXDLOW = 1 << 15, // Transmit Descriptor Low Threshold hit
    IM_SRPD = 1 << 16,   // Small Receive Packet Detection
};

#define E1000_READ_REG(hw, reg) \
    (*((volatile uint32_t *)((char *)(hw) + (reg))))

#define E1000_WRITE_REG(hw, reg, value) \
    (*((volatile uint32_t *)((char *)(hw) + (reg))) = (value))

struct rx_desc_t
{
    uint64_t addr;     // 地址
    uint16_t length;   // 长度
    uint16_t checksum; // 校验和
    uint8_t status;    // 状态
    uint8_t error;     // 错误
    uint16_t special;  // 特殊
} __attribute__((packed));

typedef struct tx_desc_t
{
    uint64_t addr;    // 缓冲区地址
    uint16_t length;  // 包长度
    uint8_t cso;      // Checksum Offset
    uint8_t cmd;      // 命令
    uint8_t status;   // 状态
    uint8_t css;      // Checksum Start Field
    uint16_t special; // 特殊
} __attribute__((packed)) tx_desc_t;

#define RX_DESC_NR 32

struct eth_device {
    char name[PCI_PRI_STR_SIZE + 1];
    int has_eeprom;
    void *hw_addr;
    uint8_t mac_addr[6];

    uint16_t rx_cur;
    uint16_t tx_cur;

    struct rx_desc_t *rx_desc;
    struct tx_desc_t *tx_desc;
};

struct eth_device *eth_device_get(const char *pci_id)
{
    char path[1024] = {0};
    struct eth_device *dev = (struct eth_device *)malloc(sizeof(struct eth_device));
    if (!dev)
        return NULL;
    snprintf(dev->name, PCI_PRI_STR_SIZE, "%s", pci_id);
    snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/resource0", pci_id);
    int fd = open(path, O_RDWR);
    dev->hw_addr = mmap(NULL, 0x1ffff, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (dev->hw_addr == MAP_FAILED) {
        free(dev);
        return NULL;
    }
    return dev;
}


/**
 * @brief   检测是否有eeprom
 * 
 * @param pci_dev 
 */
void e1000_eeprome_detect(struct eth_device *eth_dev)
{
    uint32_t *hw = (uint32_t *)eth_dev->hw_addr;
    uint32_t eerd = E1000_READ_REG(hw, E1000_EERD);
    eth_dev->has_eeprom = 0;
    if (eerd & 0x10)
        eth_dev->has_eeprom = 1;
    printf("e1000 has_eeprom: %d\n", eth_dev->has_eeprom);
    return;  
}

uint16_t e1000_eeprom_read(struct eth_device *eth_dev, uint8_t addr)
{
    uint32_t *hw = (uint32_t *)eth_dev->hw_addr;
    uint32_t eerd;
    if (eth_dev->has_eeprom) {
        eerd = E1000_WRITE_REG(hw, E1000_EERD, addr << 8 | 1);
        while(1) {
            eerd = E1000_READ_REG(hw, E1000_EERD);
            if (eerd & (1 << 4))
                break;
        }
    } else {
        // other way?
    }
    return eerd >> 16 & 0xFFFF;
}

void e1000_read_mac(struct eth_device *eth_dev)
{
    uint32_t *hw = (uint32_t *)eth_dev->hw_addr;
    uint16_t val = 0;

    if (eth_dev->has_eeprom == 0) {
        return;
    } else {
        val = e1000_eeprom_read(eth_dev, 0);
        eth_dev->mac_addr[0] = val & 0xFF;
        eth_dev->mac_addr[1] = val >> 8;

        val = e1000_eeprom_read(eth_dev, 1);
        eth_dev->mac_addr[2] = val & 0xFF;
        eth_dev->mac_addr[3] = val >> 8;

        val = e1000_eeprom_read(eth_dev, 2);
        eth_dev->mac_addr[4] = val & 0xFF;
        eth_dev->mac_addr[5] = val >> 8;
    }
    return;
}
// 初始化组播表数组
void e1000_init_multicast(struct eth_device *eth_dev)
{
    uint32_t *hw = (uint32_t *)eth_dev->hw_addr;
    for (int i = E1000_MAT0; i < E1000_MAT1; i += 4)
        E1000_WRITE_REG(hw, i, 0);
}

void e1000_intr_disable(struct eth_device *eth_dev)
{
    uint32_t *hw = (uint32_t *)eth_dev->hw_addr;
    E1000_WRITE_REG(hw, E1000_IMC, 0);
}

void e1000_intr_init(struct eth_device *eth_dev)
{
    uint32_t flags = 0;
    flags |= IM_RXT0 | IM_RXO | IM_RXDMT0 | IM_RXSEQ | IM_LSC;
    E1000_WRITE_REG(eth_dev->hw_addr, E1000_IMS, flags);
}

int e1000_rx_desc_init(struct eth_device *eth_dev)
{
    struct page *p = NULL;
    eth_dev->rx_cur = 0;

    p = alloc_page();
    if (!p) {
        printf("alloc_page failed\n");
        return -1;
    }
    eth_dev->rx_desc = (struct rx_desc_t *)p->phys_addr;
    memset(p->addr, 0, RX_DESC_NR * sizeof(struct rx_desc_t));

    // 接收描述符地址
    E1000_WRITE_REG(eth_dev->hw_addr, E1000_RDBAL, (uint32_t)((uint64_t)eth_dev->rx_desc & 0xFFFFFFFF));
    E1000_WRITE_REG(eth_dev->hw_addr, E1000_RDBAH, (uint32_t)((uint64_t)eth_dev->rx_desc >> 32));

    // 接收描述符长度
    E1000_WRITE_REG(eth_dev->hw_addr, E1000_RDLEN, RX_DESC_NR * sizeof(struct rx_desc_t));

    // 接收描述符头尾索引
    E1000_WRITE_REG(eth_dev->hw_addr, E1000_RDH, 0);
    E1000_WRITE_REG(eth_dev->hw_addr, E1000_RDT, RX_DESC_NR - 1);

    for (int i = 0; i < RX_DESC_NR; i++) {
        p = alloc_page();
        if (!p) {
            printf("alloc_page failed\n");
            return -1;
        }
        eth_dev->rx_desc[i].addr = (uint64_t)p->phys_addr;
        eth_dev->rx_desc[i].length = 2048;
        eth_dev->rx_desc[i].status = 0;
    }

    // 寄存器设置
    uint32_t flags = 0;
    flags |= RCTL_EN | RCTL_SBP | RCTL_UPE;
    flags |= RCTL_MPE | RCTL_LBM_NONE | RTCL_RDMTS_HALF;
    flags |= RCTL_BAM | RCTL_SECRC | RCTL_BSIZE_2048;

    E1000_WRITE_REG(eth_dev->hw_addr, E1000_RCTL, flags);
    return 0;
}

void e1000_reset(struct eth_device *eth_dev)
{
    e1000_eeprome_detect(eth_dev);

    e1000_read_mac(eth_dev);

    e1000_init_multicast(eth_dev);

    e1000_intr_disable(eth_dev);

    e1000_rx_desc_init(eth_dev);

    e1000_intr_init(eth_dev);
}

int e1000_init(struct eth_device *eth_dev)
{
    e1000_reset(eth_dev);
    printf("MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
           eth_dev->mac_addr[0], eth_dev->mac_addr[1],
           eth_dev->mac_addr[2], eth_dev->mac_addr[3],
           eth_dev->mac_addr[4], eth_dev->mac_addr[5]);

    // 轮询接收描述符
    while (1) {
        for (int i = 0; i < RX_DESC_NR; i++) {
            printf("rx_desc[%d].status: %d\n", i, eth_dev->rx_desc[i].status);
        }
    }
    return 0;
}





#endif
