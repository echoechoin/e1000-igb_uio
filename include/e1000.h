#ifndef _E1000_H_
#define _E1000_H_
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
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

// 接收状态
enum RS
{
    RS_DD = 1 << 0,    // Descriptor done
    RS_EOP = 1 << 1,   // End of packet
    RS_VP = 1 << 3,    // Packet is 802.1q (matched VET);
                       // indicates strip VLAN in 802.1q packet
    RS_UDPCS = 1 << 4, // UDP checksum calculated on packet
    RS_L4CS = 1 << 5,  // L4 (UDP or TCP) checksum calculated on packet
    RS_IPCS = 1 << 6,  // Ipv4 checksum calculated on packet
    RS_PIF = 1 << 7,   // Passed in-exact filter
};

// 传输控制
enum TCTL
{
    TCTL_EN = 1 << 1,      // Transmit Enable
    TCTL_PSP = 1 << 3,     // Pad Short Packets
    TCTL_CT = 4,           // Collision Threshold
    TCTL_COLD = 12,        // Collision Distance
    TCTL_SWXOFF = 1 << 22, // Software XOFF Transmission
    TCTL_RTLC = 1 << 24,   // Re-transmit on Late Collision
    TCTL_NRTU = 1 << 25,   // No Re-transmit on underrun
};

// 传输命令
enum TCMD
{
    TCMD_EOP = 1 << 0,  // When set, indicates the last descriptor making up the packet. One or many descriptors can be used to form a packet
    TCMD_IFCS = 1 << 1, // Insert FCS/CRC filed
    TCMD_IC = 1 << 2,   // Insert Checksum
    TCMD_RS = 1 << 3,   // Report Status 如果设置，将在传输完成后设置状态位
    TCMD_RPS = 1 << 4,  // Report Packet Sent
    TCMD_VLE = 1 << 6,  // VLAN Packet Enable
    TCMD_IDE = 1 << 7,  // Interrupt Delay Enable
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

// 发送状态
enum TS
{
    TS_DD = 1 << 0, // Descriptor Done
    TS_EC = 1 << 1, // Excess Collisions
    TS_LC = 1 << 2, // Late Collision
    TS_TU = 1 << 3, // Transmit Underrun
};

#define RX_DESC_NR 32
#define TX_DESC_NR 32

struct e1000_device {
    char name[PCI_PRI_STR_SIZE + 1];
    int eeprom;
    void *hw_addr;
    int uio_fd;
    int config_fd;
    uint16_t rx_cur;
    uint16_t tx_cur;
    struct rx_desc_t *rx_desc;
    struct tx_desc_t *tx_desc;
    uint8_t mac_addr[6];
};

int e1000_init(struct e1000_device *dev);
struct e1000_device *e1000_device_get(const char *pci_id);
int e1000_recv(struct e1000_device *dev, char *buf, size_t len);
int e1000_send(struct e1000_device *dev, char *buf, size_t len);

#endif
