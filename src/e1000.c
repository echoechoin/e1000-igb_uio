#include "e1000.h"
#include "mem_alloc.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <assert.h>
#include <unistd.h>

#define INTEL_82545EM_CLASS  "0x020000"
#define INTEL_82545EM_VENDOR "0x8086"
#define INTEL_82545EM_DEVICE "0x100f"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static int is_intel_82545EM(const char *pci_id)
{
    char path[1024] = {0};
    char buf[1024] = {0};
    snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/vendor", pci_id);
    FILE *fp = fopen(path, "r");
    if (!fp)
        return 0;
    fgets(buf, sizeof(buf), fp);
    fclose(fp);
    if (strncmp(buf, INTEL_82545EM_VENDOR, strlen(INTEL_82545EM_VENDOR)) != 0)
        return 0;
    snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/class", pci_id);
    fp = fopen(path, "r");
    if (!fp)
        return 0;
    fgets(buf, sizeof(buf), fp);
    fclose(fp);
    if (strncmp(buf, INTEL_82545EM_CLASS, strlen(INTEL_82545EM_CLASS)) != 0)
        return 0;
    snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/device", pci_id);
    fp = fopen(path, "r");
    if (!fp)
        return 0;
    fgets(buf, sizeof(buf), fp);
    fclose(fp);
    if (strncmp(buf, INTEL_82545EM_DEVICE, strlen(INTEL_82545EM_DEVICE)) != 0)
        return 0;
    return 1;
}

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
    struct e1000_device *dev = NULL;
    if (!is_intel_82545EM(pci_id)) // 只支持intel 82545EM 也就是VMware的默认网卡
        goto error;

    dev = (struct e1000_device *)malloc(sizeof(struct e1000_device));
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
    dev->eeprom = 0;
    for (int try = 0; try < 1000; try++) {
        uint32_t eerd = E1000_READ_REG(hw, E1000_EERD); // 返回done位
        if (eerd & 0x10) {
            dev->eeprom = 1;
            break;
        }
    }
    return;  
}

static uint16_t e1000_eeprom_read(struct e1000_device *dev, uint8_t addr)
{
    uint32_t *hw = (uint32_t *)dev->hw_addr;
    uint32_t eerd;
    if (dev->eeprom) {
        eerd = E1000_WRITE_REG(hw, E1000_EERD, (uint32_t)addr << 8 | 1); // 写start位和地址
        while(1) {
            eerd = E1000_READ_REG(hw, E1000_EERD);  // 等待done位
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
    return eerd >> 16 & 0xFFFF; // 返回数据
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
        for (int i = 0; i < 3; i++) {
            val = e1000_eeprom_read(dev, i);
            dev->mac_addr[i * 2] = val & 0xFF;
            dev->mac_addr[i * 2 + 1] = val >> 8;
        }
    }
    return;
}

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

void e1000_intr_handler(struct e1000_device *dev)
{
    printf("interrupt\n");
}

int uio_intr_enable_disable(int fd, int enable)
{
    int value = enable;
    if (write(fd, &value, sizeof(value)) < 0) {
        perror("write");
        printf("uio_intr_enable failed\n");
        exit(1);
    }
    return 0;
}

static void *e1000_intr_listen(void *arg)
{
    struct e1000_device *dev = (struct e1000_device *)arg;
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = dev->uio_fd;
    
    int epoll_fd = epoll_create(1);
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, dev->uio_fd, &event);

    while (1) {
        int ret = epoll_wait(epoll_fd, &event, 1, -1);
        if (ret < 0) {
            printf("epoll_wait failed\n");
            continue;
        }
        int value = 0;
        ret = read(dev->uio_fd, &value, sizeof(value));
        if (ret < 0) {
            perror("read");
            printf("read failed\n");
            continue;
        }
        e1000_intr_handler(dev);
    }
}

static void e1000_intr_init(struct e1000_device *dev)
{
    uint32_t flags = 0;
    // flags |= IM_RXT0 | IM_RXO | IM_RXDMT0 | IM_RXSEQ | IM_LSC;
    flags |= IM_LSC;
    E1000_WRITE_REG(dev->hw_addr, E1000_IMS, flags);

    // uio_intr_enable_disable(dev->uio_fd, 1);

    // // 创建一个线程监听中断
    // pthread_t tid;
    // pthread_create(&tid, NULL, e1000_intr_listen, dev);
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
    dev->rx_desc = addr;
    memset(addr, 0, RX_DESC_NR * sizeof(struct rx_desc_t));

    // 接收描述符地址
    struct rx_desc_t *rx_desc_phys_addr = (struct rx_desc_t *)virt_to_phys(addr);
    E1000_WRITE_REG(dev->hw_addr, E1000_RDBAL, (uint32_t)((uint64_t)rx_desc_phys_addr & 0xFFFFFFFF));
    E1000_WRITE_REG(dev->hw_addr, E1000_RDBAH, (uint32_t)((uint64_t)rx_desc_phys_addr >> 32));

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
        struct rx_desc_t *virt = phys_to_virt(rx_desc_phys_addr);
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

static int e1000_tx_desc_init(struct e1000_device *dev)
{
    void *addr = NULL;
    dev->rx_cur = 0;

    addr = alloc_page();
    if (!addr) {
        printf("alloc_page failed\n");
        return -1;
    }
    dev->tx_desc = addr;
    memset(addr, 0, TX_DESC_NR * sizeof(struct tx_desc_t));

    // 发送描述符地址
    struct tx_desc_t *tx_desc_phys_addr = (struct tx_desc_t *)virt_to_phys(addr);
    E1000_WRITE_REG(dev->hw_addr, E1000_TDBAL, (uint32_t)((uint64_t)tx_desc_phys_addr & 0xFFFFFFFF));
    E1000_WRITE_REG(dev->hw_addr, E1000_TDBAH, (uint32_t)((uint64_t)tx_desc_phys_addr >> 32));

    // 发送描述符长度
    E1000_WRITE_REG(dev->hw_addr, E1000_TDLEN, TX_DESC_NR * sizeof(struct tx_desc_t));

    // 发送描述符头尾索引
    E1000_WRITE_REG(dev->hw_addr, E1000_TDH, 0);
    E1000_WRITE_REG(dev->hw_addr, E1000_TDT, 0);

    for (int i = 0; i < TX_DESC_NR; i++) {
        addr = alloc_page();
        if (!addr) {
            printf("alloc_page failed\n");
            return -1;
        }
        struct tx_desc_t *virt = phys_to_virt(tx_desc_phys_addr);
        virt[i].addr = (uint64_t)virt_to_phys(addr);
        virt[i].cmd = 0;
        virt[i].status = TS_DD;
    }

    // 寄存器设置
    uint32_t flags = 0;
    flags |= TCTL_EN | TCTL_PSP | TCTL_RTLC;
    flags |= 0x10 << TCTL_CT;
    flags |= 0x40 << TCTL_COLD;

    E1000_WRITE_REG(dev->hw_addr, E1000_TCTL, flags);
    return 0;
}

static void e1000_reset(struct e1000_device *dev)
{
    e1000_eeprome_detect(dev);

    e1000_read_mac(dev);

    e1000_init_multicast(dev);

    e1000_intr_disable(dev);

    e1000_rx_desc_init(dev);

    e1000_tx_desc_init(dev);

    e1000_intr_init(dev);
}

int e1000_init(struct e1000_device *dev)
{
    e1000_reset(dev);
    return 0;
}

int e1000_recv(struct e1000_device *dev, char *buf, size_t len)
{
    struct rx_desc_t *desc;
    int count = 0;
    while (1) {
        desc = &dev->rx_desc[dev->rx_cur];
        if ((desc->status & RS_DD) == 0) {
            usleep(10);
            continue;
        }
        if (desc->error) {
            printf("receive error\n");
            abort();
        }
        break;
    }

    assert(desc->length < 2048);
    char *addr = phys_to_virt((void *)desc->addr);
    desc->status = 0;

    int recv_len = MIN(desc->length, len);
    memcpy(buf, addr, recv_len);

    E1000_WRITE_REG(dev->hw_addr, E1000_RDT, dev->rx_cur);
    dev->rx_cur = (dev->rx_cur + 1) % RX_DESC_NR;
    return recv_len;
}

int e1000_send(struct e1000_device *dev, char *buf, size_t len)
{
    tx_desc_t *desc = &dev->tx_desc[dev->tx_cur];

    // 等待上一个包发送完成
    while (desc->status == 0) {
        usleep(10);
    }

    assert(len < 2048);
    char *addr = phys_to_virt((void *)desc->addr);
    // 填充数据
    memcpy(addr, buf, len);

    // 设置长度
    desc->length = len;

    // 设置cmd
    desc->cmd |= TCMD_EOP | TCMD_RS | TCMD_RPS | TCMD_IFCS;
    desc->status = 0;

    // 设置发送队列尾索引
    dev->tx_cur = (dev->tx_cur + 1) % TX_DESC_NR;
    E1000_WRITE_REG(dev->hw_addr, E1000_TDT, dev->tx_cur);
    return 0;
}
