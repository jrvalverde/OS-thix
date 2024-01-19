#ifndef _THIX_VMALLOC_H
#define _THIX_VMALLOC_H


#define M_PAGE_FULL     0x00
#define M_PAGE_EMPTY    0xFF


extern int __vm_alloc_dummy__;


void  vm_alloc_init(void);
void *vm_alloc(size_t size, int (*get_dedicated_page)(void), int *type);
void  vm_free(void *address, void (*release_dedicated_page)(int));

#define vm_xalloc(size) vm_alloc(size, NULL, &__vm_alloc_dummy__)
#define vm_xfree(address) vm_free(address, NULL)


#define can_do_vm_alloc(size) (m_vect[bsf(size >> 9) + 1])


#endif  /* _THIX_VMALLOC_H */
