#ifndef _THIX_VMDEV_H
#define _THIX_VMDEV_H


#ifndef _THIX_TYPES_H
#include <thix/types.h>
#endif


#define MAX_VM_DEVICES  32


#define FS_DEVICE        1
#define SWAP_DEVICE      2


typedef struct
{
    __dev_t device;             /* Major/minor device numbers.  */
    char    type;               /* vmdev type (FS_DEVICE or SWAP_DEVICE).  */
    char    in_use;             /* in_use flag.  */
    int     aux;                /* Auxiliary info.  */
} vmdev_struct;


extern vmdev_struct vmdevt[];


void  vmdev_init(void);

#define vmdev_device(vmdev)     (vmdevt[vmdev].device)
#define vmdev_type(vmdev)       (vmdevt[vmdev].type)
#define vmdev_aux(vmdev)        (vmdevt[vmdev].aux)

int get_vmdev(__dev_t device, char type, int aux);
void release_vmdev(int vmdev);


#endif  /* _THIX_VMDEV_H */
