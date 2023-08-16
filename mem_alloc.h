#ifndef _MEM_ALLOC_H_
#define _MEM_ALLOC_H_

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

struct page {
    void *addr;
    void *phys_addr;
};

// 通过/proc/self/pagemap获取物理地址
void *get_phys_addr(void *addr)
{
    int fd = open("/proc/self/pagemap", O_RDONLY);
    if (fd < 0) {
        return NULL;
    }

    // 页框大小
    int page_size = getpagesize();
    // 页框号
    unsigned long page_num = (unsigned long)addr / page_size;
    // 页内偏移
    unsigned long offset = (unsigned long)addr % page_size;
    // 页框号 * 页框大小 + 页内偏移 = 虚拟地址
    unsigned long phys_addr = page_num * page_size + offset;

    // 移动文件指针到页框号 * 8
    lseek(fd, page_num * 8, SEEK_SET);
    // 读取8个字节
    unsigned long phys_page_num;
    read(fd, &phys_page_num, 8);
    // 低55位为物理页框号
    phys_page_num &= ((1UL << 55) - 1);
    // 物理地址 = 物理页框号 * 页框大小 + 页内偏移
    phys_addr = phys_page_num * page_size + offset;

    close(fd);
    return (void *)phys_addr;
}

struct page *alloc_page() {
    // posix_memalign
    void *addr;
    int page_size = getpagesize();
    posix_memalign(&addr, page_size, page_size);
    struct page *p = (struct page *)malloc(sizeof(struct page));
    p->addr = addr;
    p->phys_addr = get_phys_addr(addr);
    if (p->phys_addr == NULL) {
        free(p);
        return NULL;
    }
    return p;
}

void *free_page(struct page *p) {
    free(p->addr);
    free(p);
}

#endif