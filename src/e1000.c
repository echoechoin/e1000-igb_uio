#include "e1000.h"
#include "mem_alloc.h"
#include <stdint.h>
#include <stdio.h>
#include <sys/epoll.h>

/**
 * @brief 通过pci_id获取e1000设备
 * 
 * @param pci_id: 0000:02:02.0 
 */
struct e1000_device *e1000_device_get(const char *pci_id)
{
    char path[1024] = {0};
    int resource_fd = -1;
    int uio_fd = -1;
    int config_fd = -1;
    struct e1000_device *dev = (struct e1000_device *)malloc(sizeof(struct e1000_device));
    if (!dev)
        goto error;
    snprintf(dev->name, PCI_PRI_STR_SIZE, "%s", pci_id);
    snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/resource0", pci_id);
    resource_fd = open(path, O_RDWR);
    if (resource_fd < 0)
        goto error;
    dev->hw_addr = mmap(NULL, 0x1ffff, PROT_READ | PROT_WRITE, MAP_SHARED, resource_fd, 0);
    if (dev->hw_addr == MAP_FAILED)
        goto error;
    snprintf(path, sizeof(path), "/dev/uio0");
    uio_fd = open(path, O_RDWR);
    if (uio_fd < 0)
        goto error;
    snprintf(path, sizeof(path), "/sys/class/uio/uio0/device/config");
    config_fd = open(path, O_RDWR);
    if (config_fd < 0)
        goto error;
    dev->uio_fd = uio_fd;
    dev->config_fd = config_fd;
    return dev;

error:
    if (resource_fd >= 0)
        close(resource_fd);
    if (uio_fd >= 0)
        close(uio_fd);
    if (config_fd >= 0)
        close(config_fd);
    if (dev)
        free(dev);
    return NULL;
}

static void e1000_eeprome_detect(struct e1000_device *dev)
{
    uint32_t *hw = (uint32_t *)dev->hw_addr;
    E1000_WRITE_REG(hw, E1000_EERD, 1); // 写start位
    for (int try = 0; try < 1000; try++) {
        uint32_t eerd = E1000_READ_REG(hw, E1000_EERD); // 返回done位
        dev->eeprom = 0;
        if (eerd & 0x10)
            dev->eeprom = 1;
    }
    return;  
}

static uint16_t e1000_eeprom_read(struct e1000_device *dev, uint8_t addr)
{
    uint32_t *hw = (uint32_t *)dev->hw_addr;
    uint32_t eerd;
    if (dev->eeprom) {
        eerd = E1000_WRITE_REG(hw, E1000_EERD, (uint32_t)addr << 8 | 1);
        while(1) {
            eerd = E1000_READ_REG(hw, E1000_EERD);
            if (eerd & (1 << 4))
                break;
        }
    } else {
        eerd = E1000_WRITE_REG(hw, E1000_EERD, (uint32_t)addr << 2 | 1);
        while(1) {
            eerd = E1000_READ_REG(hw, E1000_EERD);
            if (eerd & (1 << 1))
                break;
        }
    }
    return eerd >> 16 & 0xFFFF;
}

static void e1000_read_mac(struct e1000_device *dev)
{
    uint32_t *hw = (uint32_t *)dev->hw_addr;
    uint16_t val = 0;

    if (dev->eeprom == 0) {
        char *mac = (char *)hw + 0x5400;
        for (int i = 0; i < 6; i++)
            dev->mac_addr[i] = mac[i];
    } else {
        val = e1000_eeprom_read(dev, 0);
        dev->mac_addr[0] = val & 0xFF;
        dev->mac_addr[1] = val >> 8;

        val = e1000_eeprom_read(dev, 1);
        dev->mac_addr[2] = val & 0xFF;
        dev->mac_addr[3] = val >> 8;

        val = e1000_eeprom_read(dev, 2);
        dev->mac_addr[4] = val & 0xFF;
        dev->mac_addr[5] = val >> 8;
    }
    return;
}
// 初始化组播表数组
static void e1000_init_multicast(struct e1000_device *dev)
{
    uint32_t *hw = (uint32_t *)dev->hw_addr;
    for (int i = E1000_MAT0; i < E1000_MAT1; i += 4)
        E1000_WRITE_REG(hw, i, 0);
}

static void e1000_intr_disable(struct e1000_device *dev)
{
    uint32_t *hw = (uint32_t *)dev->hw_addr;
    E1000_WRITE_REG(hw, E1000_IMC, 0);
}

static void e1000_intr_init(struct e1000_device *dev)
{
    uint32_t flags = 0;
    // flags |= IM_RXT0 | IM_RXO | IM_RXDMT0 | IM_RXSEQ | IM_LSC;
    flags |= IM_LSC;
    E1000_WRITE_REG(dev->hw_addr, E1000_IMS, flags);

    // uio中断处理
}

static int e1000_rx_desc_init(struct e1000_device *dev)
{
    void *addr = NULL;
    dev->rx_cur = 0;

    addr = alloc_page();
    if (!addr) {
        printf("alloc_page failed\n");
        return -1;
    }
    dev->rx_desc = (struct rx_desc_t *)virt_to_phys(addr);
    memset(addr, 0, RX_DESC_NR * sizeof(struct rx_desc_t));

    // 接收描述符地址
    E1000_WRITE_REG(dev->hw_addr, E1000_RDBAL, (uint32_t)((uint64_t)dev->rx_desc & 0xFFFFFFFF));
    E1000_WRITE_REG(dev->hw_addr, E1000_RDBAH, (uint32_t)((uint64_t)dev->rx_desc >> 32));

    // 接收描述符长度
    E1000_WRITE_REG(dev->hw_addr, E1000_RDLEN, RX_DESC_NR * sizeof(struct rx_desc_t));

    // 接收描述符头尾索引
    E1000_WRITE_REG(dev->hw_addr, E1000_RDH, 0);
    E1000_WRITE_REG(dev->hw_addr, E1000_RDT, RX_DESC_NR - 1);

    for (int i = 0; i < RX_DESC_NR; i++) {
        addr = alloc_page();
        if (!addr) {
            printf("alloc_page failed\n");
            return -1;
        }
        struct rx_desc_t *virt = phys_to_virt(dev->rx_desc);
        virt[i].addr = (uint64_t)virt_to_phys(addr);
        virt[i].length = 2048;
        virt[i].status = 0;
    }

    // 寄存器设置
    uint32_t flags = 0;
    flags |= RCTL_EN | RCTL_SBP | RCTL_UPE;
    flags |= RCTL_MPE | RCTL_LBM_NONE | RTCL_RDMTS_HALF;
    flags |= RCTL_BAM | RCTL_SECRC | RCTL_BSIZE_2048;

    E1000_WRITE_REG(dev->hw_addr, E1000_RCTL, flags);
    return 0;
}

static void e1000_reset(struct e1000_device *dev)
{
    e1000_eeprome_detect(dev);

    e1000_read_mac(dev);

    e1000_init_multicast(dev);

    e1000_intr_disable(dev);

    e1000_rx_desc_init(dev);

    e1000_intr_init(dev);
}

int e1000_init(struct e1000_device *dev)
{
    e1000_reset(dev);
    return 0;
}