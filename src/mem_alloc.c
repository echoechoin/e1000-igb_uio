

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include "mem_alloc.h"

#define PAGE_TABLE_NR 1024
static struct page page_table[PAGE_TABLE_NR];

static struct page *get_free_page() {
    for (int i = 0; i < PAGE_TABLE_NR; i++) {
        if (page_table[i].addr == NULL) {
            return &page_table[i];
        }
    }
    return NULL;
}

// 通过/proc/self/pagemap获取物理地址
static void *get_phys_addr(void *virt_addr)
{
    uint64_t page_frame_num;
    int fd = open("/proc/self/pagemap", O_RDONLY);
    if (fd < 0) {
        return NULL;
    }

    int page_size = getpagesize();

    off_t offset = ((uint64_t)virt_addr / page_size) * sizeof(uint64_t);

    // Seek to the offset
    if (lseek(fd, offset, SEEK_SET) == (off_t)-1) {
        printf("lseek failed\n");
        close(fd);
        return NULL;
    }

    // Read the page frame number from the pagemap
    if (read(fd, &page_frame_num, sizeof(uint64_t)) != sizeof(uint64_t)) {
        printf("read failed\n");
        close(fd);
        return NULL;
    }

    page_frame_num &= ((1ULL << 55) - 1);
    uint64_t phys_addr = ((page_frame_num & 0x7fffffffffffffULL) * page_size)
                + ((unsigned long)virt_addr % page_size);
    return (void *)phys_addr;
}

struct page *alloc_page() {
    // posix_memalign
    void *addr;
    int page_size = getpagesize();
    posix_memalign(&addr, page_size, page_size);
    memset(addr, 0, page_size);
    struct page *p = get_free_page();
    p->addr = addr;
    p->phys_addr = get_phys_addr(addr);
    if (p->phys_addr == NULL) {
        free(p);
        return NULL;
    }
    printf("alloc_page: virt_addr=%p, phys_addr=%p\n", addr, p->phys_addr);
    return p->addr;
}

void free_page(struct page *p) {
    free(p->addr);
    p->addr = NULL;
    p->phys_addr = NULL;
}

void *phys_to_virt(void *phys_addr)
{
    for (int i = 0; i < PAGE_TABLE_NR; i++) {
        if (page_table[i].phys_addr == phys_addr) {
            return page_table[i].addr;
        }
    }
    return NULL;
}

void *virt_to_phys(void *virt_addr)
{
    for (int i = 0; i < PAGE_TABLE_NR; i++) {
        if (page_table[i].addr == virt_addr) {
            return page_table[i].phys_addr;
        }
    }
    return NULL;
}
