#ifndef _THIX_PART_H
#define _THIX_PART_H


#ifndef _THIX_TYPES_H
#include <thix/types.h>
#endif


typedef struct
{
    unsigned first_block;       /* 1 block = 1 physical sector.  */
    unsigned total_blocks;
} partition;


int partitions_setup(block_driver *bd, __dev_t device, partition *p,
		     int cylinder, int track, int sectors);


#endif  /* _THIX_PART_H */
