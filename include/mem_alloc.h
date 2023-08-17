#ifndef _MEM_ALLOC_H_
#define _MEM_ALLOC_H_

struct page {
    void *addr;
    void *phys_addr;
};

struct page *alloc_page();
void free_page(struct page *p);

void *phys_to_virt(void *phys_addr);
void *virt_to_phys(void *virt_addr);

#endif