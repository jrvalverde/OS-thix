#ifndef _THIX_BUFFER_H
#define _THIX_BUFFER_H


#ifndef _THIX_TYPES_H
#include <thix/types.h>
#endif

#ifndef _THIX_VMALLOC_H
#include <thix/vmalloc.h>
#endif


#define MAX_BUFFERS                     4096
#define BUFFERS_BUCKETS_NO               199
#define SYNC_ALL_DEVICES        ((__dev_t)0)


typedef struct
{
    unsigned block         : 24,
	     busy          : 1,
	     in_demand     : 1,
	     delayed_write : 1,
	     valid         : 1,
	     dedicated     : 1;

    __dev_t device;
    unsigned short size;

    unsigned char *address;

    unsigned short hprev;
    unsigned short hnext;

    unsigned short lprev;
    unsigned short lnext;

    unsigned short next;
    unsigned short __fill;
} buffer_t;


extern buffer_t *buf_vect;


#define buf_address(buf_no)     buf_vect[buf_no].address


void buf_init(void);
int  buf_cleanup(void);

int  buf_get(__dev_t device, int block, size_t size);
void buf_release(int buf_no);

int  buf_read(__dev_t device, int block, size_t size);
int  _buf_read(int buf_no);
void buf_write(int buf_no);

int  buf_hash_search(__dev_t device, int block);

void buf_shrink(int free_pages_request);
void buf_sync(__dev_t device, int remove);


#endif  /* _THIX_BUFFER_H */
