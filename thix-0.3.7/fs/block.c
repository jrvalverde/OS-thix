/* block.c -- File system block allocation routines.  */

/*
 * Thix Operating System
 * Copyright (C) 1993,1996 Tudor Hulubei
 *
 * This program is  free software; you can  redistribute it and/or modify
 * it under the terms  of the GNU General Public  License as published by
 * the  Free Software Foundation; either version  2,  or (at your option)
 * any later version.
 *
 * This program is distributed  in the hope that  it will be  useful, but
 * WITHOUT    ANY  WARRANTY;  without   even   the  implied  warranty  of
 * MERCHANTABILITY  or  FITNESS FOR  A PARTICULAR   PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should  have received a  copy of  the GNU  General Public  License
 * along with   this  program; if   not,  write  to   the Free   Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include <thix/vm.h>
#include <thix/klib.h>
#include <thix/string.h>
#include <thix/sleep.h>
#include <thix/sched.h>
#include <thix/buffer.h>
#include <thix/block.h>
#include <thix/cluster.h>
#include <thix/gendrv.h>


/*
 * Try to lock the superblock for file operations.  Wait until the
 * current process that keeps the lock releases it.
 */

void
sb_f_lock(superblock *sb)
{
    while (sb->s_flock)
    {
	sb->s_f_in_demand = 1;
	sleep(&sb->s_flock, WAIT_SUPERBLOCK);
    }

    sb->s_flock = 1;
}


/*
 * Release the superblock lock for file operations.  Wakes up all the
 * processes currently waiting for the lock to be released.
 */

void
sb_f_unlock(superblock *sb)
{
    sb->s_flock = 0;

    if (sb->s_f_in_demand)
    {
	wakeup(&sb->s_flock);
	sb->s_f_in_demand = 0;
    }
}


/*
 * Allocate a new file system block from a file system.
 */

int
b_alloc(__dev_t device)
{
    int buf_no, block, mp;
    superblock *sb = sb(device);


    sb_f_lock(sb);

    mp = mp(device);

    if (mpt[mp].bcount == 0)
    {
	DEBUG(2, "no more free blocks in cluster %d\n", mpt[mp].bcluster);

	if (search_cluster(mp, TFS_BLOCKS_BITMAP) == 0)
	{
	    sb_f_unlock(sb);
	    return 0;
	}
    }

    block = get_bitmap_block(mpt[mp].bbitmap,
			     cluster_data_blocks(mpt[mp].bcluster) / 8);

    block = cluster2fs(mpt[mp].bcluster, block);

    mpt[mp].bcount--;
    sb->s_bfree--;
    sb->s_fmod = 1;
    sb_f_unlock(sb);

    buf_no = buf_get(device, block, sb->s_blksz);
    lmemset(buf_vect[buf_no].address, 0, sb->s_blksz >> 2);
    return buf_no;
}


/*
 * Free a block from a file system.  The just freed file system block can be
 * reallocated.
 */

void
b_free(__dev_t device, int block)
{
    int mp, buf_no;
    int new_cluster;
    superblock *sb = sb(device);


    sb_f_lock(sb);

    mp = mp(device);

    new_cluster = (block - (1+1+sb->s_ssbldsz+sb->s_bblsz)) / sb->s_bpcluster;

    if (new_cluster != mpt[mp].bcluster)
	switch_cluster(mp, new_cluster, TFS_BLOCKS_BITMAP, 1);

    release_bitmap_block(mpt[mp].bbitmap, fs2cluster(block));
    mpt[mp].bcount++;

    if ((buf_no = buf_hash_search(device, block)))
    {
	buf_vect[buf_no].valid         = 0;
	buf_vect[buf_no].delayed_write = 0;
    }

    sb->s_bfree++;
    sb->s_fmod = 1;
    sb_f_unlock(sb);
}
