/* cluster.c -- Clusters management.  */

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
#include <thix/proc/i386.h>


/*
 * A funny looking array, helping us to find out how many bits of '1'
 * are in a byte. See the count_bitmap() function below.
 */

unsigned char bits[256] =
{
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8
};


/*
 * This function counts the number of free inodes/blocks in the
 * corresponding cluster bitmap.
 */

int
count_bitmap(unsigned char *bitmap, int bitmap_size)
{
    int i, total = 0, limit = bitmap_size / 4;

    for (i = 0; i < limit; bitmap += sizeof(unsigned), i++)
    {
	if (*(unsigned *)bitmap == 0) continue;

	if (*(unsigned *)bitmap == 0xFFFFFFFF)
	    total += 32;
	else
	    total += bits[bitmap[0]] +
		     bits[bitmap[1]] +
		     bits[bitmap[2]] +
		     bits[bitmap[3]];
    }

    DEBUG(6, "counting bitmap: %x\n", total);

    return total;
}


/*
 * Search a free inode or block into a bitmap block.
 */

int
get_bitmap_resource(unsigned *bitmap, int bitmap_size)
{
    int i, index;

    for (i = 0; i < bitmap_size / 4; i++)
	if (bitmap[i])
	{
	    index = bsf(bitmap[i]);
	    bitmap[i] &= ~(1 << index);
	    return i * 32 + index;
	}

    PANIC("no more free bitmap resources");
    return 0;
}


/*
 * Mark an inode or block as free into the corresponding bitmap block.
 */

void
release_bitmap_resource(unsigned *bitmap, int resource)
{
    bitmap[resource / 32] |= (1 << (resource % 32));
}


/*
 * Write back to disk the current inodes/blocks bitmap.
 */

void
flush_cluster(int mp, int type)
{
    int buf_no;
    superblock *sb = mpt[mp].sb;


    if (type == TFS_INODES_BITMAP)
    {
	if (mpt[mp].icluster != -1)
	{
	    buf_no = buf_get(mpt[mp].device,
			     cluster_reserved2fs(mpt[mp].icluster,
						 TFS_INODES_BITMAP),
			     sb->s_blksz);
	    memcpy(buf_address(buf_no), mpt[mp].ibitmap, sb->s_blksz);
	    buf_write(buf_no);
	}
    }
    else
    {
	if (mpt[mp].bcluster != -1)
	{
	    buf_no = buf_get(mpt[mp].device,
			     cluster_reserved2fs(mpt[mp].bcluster,
						 TFS_BLOCKS_BITMAP),
			     sb->s_blksz);
	    memcpy(buf_address(buf_no), mpt[mp].bbitmap, sb->s_blksz);
	    buf_write(buf_no);
	}
    }
}


/*
 * Try to find a cluster containing free resources of the given type.
 */

int
switch_cluster(int mp, int cluster, int type, int force)
{
    int buf_no;
    unsigned *address;
    int i, limit, count;
    superblock *sb = mpt[mp].sb;


    buf_no = buf_read(mpt[mp].device, cluster_reserved2fs(cluster, type),
		      sb->s_blksz);

    if (buf_no == 0)
	if (!force)
	    return 0;
	else
	    PANIC("can't switch clusters");

    address = (unsigned *)buf_address(buf_no);

    if (type == TFS_INODES_BITMAP)
	limit = sb->s_icluster / 32;
    else
	limit = cluster_data_blocks(cluster) / 32;

    if (force)
	goto do_switch_cluster;

    for (i = 0; i < limit; i++)
	if (address[i])
	{
	  do_switch_cluster:
	    count = count_bitmap((unsigned char *)address,
				 limit * sizeof(unsigned));

	    flush_cluster(mp, type);

	    if (type == TFS_INODES_BITMAP)
	    {
		memcpy(mpt[mp].ibitmap, address, limit * sizeof(unsigned));
		mpt[mp].icluster = cluster;
		mpt[mp].icount   = count;
	    }
	    else
	    {
		memcpy(mpt[mp].bbitmap, address, limit * sizeof(unsigned));
		mpt[mp].bcluster = cluster;
		mpt[mp].bcount   = count;
	    }

	    buf_release(buf_no);
	    return 1;
	}

    buf_release(buf_no);
    return 0;
}


/*
 * type == TFS_INODES_BITMAP:
 *    Search a cluster with at least one free inode. If it finds one and
 * this cluster has a reasonable number of free blocks, change the block
 * cluster too. This function should be  called  if  the  current  inode
 * cluster has no free inodes or if this  is  the  first time we want to
 * allocate a new inode (mpt[mp].icluster == -1).
 *
 * type == TFS_BLOCKS_BITMAP:
 *    Read the comment above, reversing inode & block. :-)
 */

int
search_cluster(int mp, int type)
{
    int cluster;        /* cluster number. */
    int old_cluster;    /* old cluster. */
    int first_time;     /* first time flag. */
    superblock *sb;     /* pointer to the superblock. */


    sb = mpt[mp].sb;

    /* We must check first if we do have free resources in the file system.  */

    if (type == TFS_INODES_BITMAP && sb->s_ifree == 0) return 0;
    if (type == TFS_BLOCKS_BITMAP && sb->s_bfree == 0) return 0;

    /* Ok, we seem to have some...  */

    old_cluster = (type == TFS_INODES_BITMAP) ? mpt[mp].icluster :
						mpt[mp].bcluster;

    cluster = old_cluster + 1;

    DEBUG(6, "old_cluster=%d, cluster=%d\n", old_cluster, cluster);

    if (old_cluster == -1)
	first_time = 1;
    else
    {
	first_time = 0;
	if (cluster == sb->s_clusters) cluster = 0;
    }

    for (; cluster < sb->s_clusters; cluster++)
	if (switch_cluster(mp, cluster, type, 0))
	    goto found;

    if (first_time)
	goto fail;

    for (cluster = 0; cluster < old_cluster; cluster++)
	if (switch_cluster(mp, cluster, type, 0))
	    goto found;

  fail:

    PANIC("no more %s", (type == TFS_INODES_BITMAP) ? "inodes" : "blocks");

  found:

    /*
     * We should try here to change the cluster for the other kind of
     * resource (inode or block).  Later :-)...
     */

    return 1;
}
