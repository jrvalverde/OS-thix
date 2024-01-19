/* inode.c -- Inodes management routines.  */

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

/*
 * i_list_insert(), i_list_delete(), i_hash_insert() & i_hash_delete()
 * by Andrei Pitis.
 */


#include <thix/vm.h>
#include <thix/klib.h>
#include <thix/string.h>
#include <thix/sched.h>
#include <thix/buffer.h>
#include <thix/inode.h>
#include <thix/block.h>
#include <thix/stat.h>
#include <thix/gendrv.h>
#include <thix/generic.h>
#include <thix/cluster.h>


#define ERR_BADLVL              error[0]
#define ERR_BADROOT             error[1]
#define ERR_HOLEINDIR           error[2]

static char *error[] =
{
    "bad level",
    "root inode has no links",
    "hole in directory",
};


extern int root_inode;
extern int root_device;


/* This can be optimized by having BLOCKS_PER_IBLOCK a local variable.  */
#define BLOCKS_PER_IBLOCK       (sb->s_blksz >> 2)


/*
 * This macro computes the block number for a given inode.
 */

#define iBLOCK(i_no)                                                    \
     (1 + 1 + sb->s_ssbldsz + sb->s_bblsz +                             \
     (i_vect[i_no].dinode / sb->s_icluster) * sb->s_bpcluster +         \
     1 + 1 + 1 +                                                        \
     ((i_vect[i_no].dinode % sb->s_icluster) * sizeof(dinode_t)) / sb->s_blksz)

/*
 * This macro computes the offset in its block number for a given inode.
 */

#define iOFFSET(i_no)                                                   \
    (((i_vect[i_no].dinode % sb->s_icluster) * sizeof(dinode_t)) % sb->s_blksz)


dinode_t dummy_inode =
{
    S_IFREG,    /* di_mode. */
    1,          /* di_nlinks. */
    0,          /* di_uid. */
    0,          /* di_gid. */
    0,          /* di_size. */
    0,          /* di_blocks. */
    {},         /* di_addr[]. */
    0,          /* di_pipesize. */
    0,          /* di_atime. */
    0,          /* di_mtime. */
    0,          /* di_ctime. */
    0,          /* di_rdev. */
    0,          /* di_pipercnt. */
    0,          /* di_pipewcnt. */
    0,          /* di_piperoff. */
    0,          /* di_pipewoff. */
};


int free_inodes = 0;
inode_t *i_vect;


#define MAX_FS_BLOCK    2048

unsigned i_zero[MAX_FS_BLOCK / sizeof(unsigned)];


/*
 * Insert an inode into the inode list.
 */

inline void
i_list_insert(int i_no)
{
    i_vect[i_no].lprev = i_vect[0].lprev;
    i_vect[i_no].lnext = 0;
    i_vect[i_vect[0].lprev].lnext = i_no;
    i_vect[0].lprev = i_no;
    free_inodes++;
}


/*
 * Delete an inode from the inode list.
 */

inline void
i_list_delete(int i_no)
{
    i_vect[i_vect[i_no].lprev].lnext = i_vect[i_no].lnext;
    i_vect[i_vect[i_no].lnext].lprev = i_vect[i_no].lprev;
    free_inodes--;
}


/*
 * Insert an inode into the inode hash table.
 */

inline void
i_hash_insert(int i_no)
{
    int index = MAX_INODES +
		(i_vect[i_no].device ^ i_vect[i_no].dinode) %
		INODES_BUCKETS_NO;

    i_vect[i_no].hnext = i_vect[index].hnext;
    i_vect[i_no].hprev = index;
    i_vect[i_vect[index].hnext].hprev = i_no;
    i_vect[index].hnext = i_no;
}


/*
 * Delete an inode from the inode hash table.
 */

inline void
i_hash_delete(int i_no)
{
    if (i_vect[i_no].hnext == 0xFFFF)
	return;

    i_vect[i_vect[i_no].hprev].hnext = i_vect[i_no].hnext;
    i_vect[i_vect[i_no].hnext].hprev = i_vect[i_no].hprev;
    i_vect[i_no].hnext = 0xFFFF;
}


/*
 * Search an inode into the inode hash table.
 */

int
i_hash_search(__dev_t device, __ino_t dinode)
{
    int i_no, index = MAX_INODES + (device ^ dinode) % INODES_BUCKETS_NO;

    for (i_no = i_vect[index].hnext;
	 (i_no != index) &&
	 (i_vect[i_no].device != device || i_vect[i_no].dinode != dinode);
	 i_no = i_vect[i_no].hnext);

    return i_no == index ? 0 : i_no;
}


/*
 * Read an inode from the file system.
 */

inline void
i_read(superblock *sb, int i_no)
{
    int buf_no;

    buf_no = buf_read(i_vect[i_no].device, iBLOCK(i_no), sb->s_blksz);

    if (buf_no)
    {
	*(dinode_t *)&i_vect[i_no] = *(dinode_t *)(buf_address(buf_no) +
						   iOFFSET(i_no));
	buf_release(buf_no);
    }
    else
	*(dinode_t *)&i_vect[i_no] = dummy_inode;
}


/*
 * Write an inode back to the file system.
 */

inline void
i_write(superblock *sb, int i_no)
{
    int buf_no = buf_read(i_vect[i_no].device, iBLOCK(i_no), sb->s_blksz);

    if (buf_no)
    {
	*(dinode_t *)(buf_address(buf_no) + iOFFSET(i_no)) =
	    *(dinode_t *)&i_vect[i_no];

	buf_write(buf_no);
    }

    i_vect[i_no].modified = 0;
}


/*
 * Lock the superblock for inode operations.
 */

void
sb_i_lock(superblock *sb)
{
    while (sb->s_ilock)
    {
	sb->s_i_in_demand = 1;
	sleep(&sb->s_ilock, WAIT_SUPERBLOCK);
    }

    sb->s_ilock = 1;
}


/*
 * Unlock the superblock for inode operations.
 */

void
sb_i_unlock(superblock *sb)
{
    sb->s_ilock = 0;

    if (sb->s_i_in_demand)
    {
	wakeup(&sb->s_ilock);
	sb->s_i_in_demand = 0;
    }
}


/*
 * Get an inode structure for the given disk inode.
 */

int
i_get(__dev_t device, __ino_t dinode)
{
    int i_no;

    for (;;)
    {
	if ((i_no = i_hash_search(device, dinode)))
	{
	    if (i_vect[i_no].busy)
	    {
		i_vect[i_no].in_demand = 1;
		sleep(&i_vect[i_no], WAIT_INODE);
		continue;
	    }

	    if (i_vect[i_no].mount_point)
	    {
		int mp = find_mp(i_no, 0);
		device = mpt[mp].device;
		dinode = ROOT_INODE;
		DEBUG(6, "mp=%x for mp_i_no=%x device=%x\n", mp, i_no, device);
		continue;
	    }

	    i_vect[i_no].busy = 1;

	    if (i_vect[i_no].nref++ == 0)
		i_list_delete(i_no);

	    return i_no;
	}

	if (free_inodes == 0)
	    return 0;

	i_vect[i_no].busy      = 1;
	i_vect[i_no].in_demand = 0;

	i_list_delete(i_no = i_vect[0].lnext);

	i_hash_delete(i_no);
	i_vect[i_no].device = device;
	i_vect[i_no].dinode = dinode;
	i_read(sb(device), i_no);
	i_hash_insert(i_no);

	i_vect[i_no].nref         = 1;
	i_vect[i_no].write_denied = 0;
	i_vect[i_no].exec_denied  = 0;
	i_vect[i_no].mount_point  = 0;

	return i_no;
    }
}


/*
 * Release an inode structure.
 */

void
i_release(int i_no)
{
    if (i_vect[i_no].dinode == 0)
	PANIC("trying to releasing inode 0\n");

    i_lock(i_no);

    if (--i_vect[i_no].nref == 0)
    {
	if (i_vect[i_no].i_nlinks == 0)
	{
	    i_truncate(i_no, 0);
	    i_vect[i_no].i_mode = 0;
	    i_free(i_vect[i_no].device, i_vect[i_no].dinode);
	}

	if (i_vect[i_no].modified)
	    i_write(sb(i_vect[i_no].device), i_no);

	i_list_insert(i_no);
    }

    i_unlock(i_no);
}


/*
 * Remove an inode from the inode cache (hash table) if possible (nref
 * == 0).  This way we can be sure that nobody will use it to refer to
 * the corresponding disk inode again.  Useful when unmounting file
 * systems.  Return the number of references to the inode.  If 0, the
 * inode has been succesfully removed.
 */

int
i_remove(int i_no)
{
    if (i_vect[i_no].nref == 0)
	i_hash_delete(i_no);

    return i_vect[i_no].nref;
}


/*
 * Allocate a new disk inode from the specified device.
 */

int
i_alloc(__dev_t device)
{
    __ino_t dinode;
    int mp, i_no;
    superblock *sb = sb(device);


    sb_i_lock(sb);

    mp = mp(device);

    if (mpt[mp].icount == 0)
    {
	if (search_cluster(mp, TFS_INODES_BITMAP) == 0)
	{
	    sb_i_unlock(sb);
	    return NO_INODE;
	}
    }

    dinode = get_bitmap_inode(mpt[mp].ibitmap, sb->s_icluster / 8);

    /* FIXME: this should be a macro similar with cluster2fs but for
       inodes.  */
    dinode = mpt[mp].icluster * sb->s_icluster + dinode;

    mpt[mp].icount--;

    sb->s_ifree--;
    sb->s_fmod = 1;
    sb_i_unlock(sb);

    i_no = i_get(device, dinode);

    i_vect[i_no].i_pipesize = 0;
    i_vect[i_no].i_pipercnt = 0;
    i_vect[i_no].i_pipewcnt = 0;
    i_vect[i_no].i_piperoff = 0;
    i_vect[i_no].i_pipewoff = 0;

    i_vect[i_no].i_size   = 0;
    i_vect[i_no].i_rdev   = 0;
    i_vect[i_no].i_nlinks = 1;
    i_vect[i_no].i_size   = 0;
    i_vect[i_no].i_blocks = 0;
    i_vect[i_no].i_mode   = 0;

    i_vect[i_no].i_ktime = seconds;

    i_set_time(i_no);

    lmemset(i_vect[i_no].i_addr, 0, sizeof(i_vect[i_no].i_addr) >> 2);
    DEBUG(6, "INODE: i_no=%x inode=%x free=%x\n", i_no, dinode, sb->s_ifree);
    i_write(sb, i_no);
    return i_no;
}


/*
 * Free the given disk inode.
 */

void
i_free(__dev_t device, __ino_t dinode)
{
    int mp;
    int new_cluster;
    superblock *sb = sb(device);


    sb_i_lock(sb);

    mp = mp(device);

    new_cluster = dinode / sb->s_icluster;

    if (new_cluster != mpt[mp].icluster)
	switch_cluster(mp, new_cluster, TFS_INODES_BITMAP, 1);

    /* FIXME: this should be a macro similar with fs2cluster but for
       inodes.  */
    release_bitmap_inode(mpt[mp].ibitmap, dinode % sb->s_icluster);
    mpt[mp].icount++;

    sb->s_ifree++;
    sb->s_fmod = 1;
    sb_i_unlock(sb);
}


/*
 * Lock the given inode structure.
 */

void
i_lock(int i_no)
{
    while (i_vect[i_no].busy)
    {
	i_vect[i_no].in_demand = 1;
	sleep(&i_vect[i_no], WAIT_INODE);
    }

    i_vect[i_no].busy = 1;
}


/*
 * Unlock the given inode structure.
 */

void
i_unlock(int i_no)
{
    i_vect[i_no].busy = 0;

    if (i_vect[i_no].in_demand)
    {
	wakeup(&i_vect[i_no]);
	i_vect[i_no].in_demand = 0;
    }
}


/*
 * Read a direct block number from the given inode.
 */

inline unsigned
get_block_from_inode(int i_no, int index)
{
    return (*(unsigned *)&i_vect[i_no].i_addr[(index<<1) + index]) & 0xFFFFFF;
}


/*
 * Write a direct block number to the given inode.
 */

inline void
put_block_to_inode(int i_no, int index, unsigned value)
{
    *(unsigned short *)&i_vect[i_no].i_addr[(index << 1) + index] = value;
    i_vect[i_no].i_addr[(index << 1) + index + 2] = value >> 16;
    i_vect[i_no].modified = 1;
}


#define set_indirection_level()                                             \
{                                                                           \
    if (fblock < DIRECT_BLOCKS) level = 0;                                  \
    else                                                                    \
	if ((fblock -= DIRECT_BLOCKS) < BLOCKS_PER_IBLOCK) level = 1;       \
	else                                                                \
	    if ((fblock -= BLOCKS_PER_IBLOCK) <                             \
		BLOCKS_PER_IBLOCK * BLOCKS_PER_IBLOCK) level = 2;           \
	    else                                                            \
		level = 3, fblock -= BLOCKS_PER_IBLOCK * BLOCKS_PER_IBLOCK; \
}                                                                           \


/*
 * Free the inode direct blocks starting with the specified block number.
 */

void
free_dlevel(int i_no, int fblock)
{
    int b, device = i_vect[i_no].device, block;

    for (b = fblock; b < DIRECT_BLOCKS; b++)
	if ((block = get_block_from_inode(i_no, b)))
	{
	    b_free(device, block);
	    i_vect[i_no].i_blocks--;
	    put_block_to_inode(i_no, b, 0);
	}
}


/*
 * Free the first level inode indirect blocks starting with the specified
 * block number.
 */

int
free_ilevel1(superblock *sb, int i_no, __dev_t device, int iblock,
	     int start_index)
{
    unsigned *address;
    int index, buf_no, block;

    if ((buf_no = buf_read(device, iblock, sb->s_blksz)) == 0)
	return 0;

    for (index = start_index; index < BLOCKS_PER_IBLOCK; index++)
	if ((block = *(address = (unsigned*)(buf_address(buf_no) +
					     (index<<2)))))
	{
	    if (start_index) *address = 0;
	    b_free(device, block);
	    i_vect[i_no].i_blocks--;
	}

    if (start_index)
	buf_write(buf_no);
    else
    {
	buf_release(buf_no);
	b_free(device, iblock);
	i_vect[i_no].i_blocks--;
    }

    return !start_index;
}


/*
 * Free the second level inode indirect blocks starting with the specified
 * block number.
 */

int
free_ilevel2(superblock *sb, int i_no, __dev_t device, int iblock,
	     int start_index, int fblock)
{
    unsigned *address;
    int next_start_index = fblock % BLOCKS_PER_IBLOCK;
    int index, buf_no, block, partial = start_index || next_start_index;

    if ((buf_no = buf_read(device, iblock, sb->s_blksz)) == 0)
	return 0;

    for (index = start_index; index < BLOCKS_PER_IBLOCK; index++)
	if ((block = *(address = (unsigned *)(buf_address(buf_no) +
					      (index<<2)))))
	{
	    if (partial) *address = 0;
	    free_ilevel1(sb, i_no, device, block,
			 (index == start_index) ? next_start_index : 0);
	}

    if (partial)
	buf_write(buf_no);
    else
    {
	buf_release(buf_no);
	b_free(device, iblock);
	i_vect[i_no].i_blocks--;
    }

    return !partial;
}


/*
 * Free the third level inode indirect blocks starting with the specified
 * block number.
 */

int
free_ilevel3(superblock *sb, int i_no, __dev_t device, int iblock,
	     int start_index, int fblock)
{
    unsigned *address;
    int index, buf_no, block, next_start_index = fblock / BLOCKS_PER_IBLOCK;
    int partial = start_index || fblock%BLOCKS_PER_IBLOCK || next_start_index;

    if ((buf_no = buf_read(device, iblock, sb->s_blksz)) == 0)
	return 0;

    for (index = start_index; index < BLOCKS_PER_IBLOCK; index++)
	if ((block = *(address = (unsigned *)(buf_address(buf_no) +
					      (index<<2)))))
	{
	    if (partial) *address = 0;
	    free_ilevel2(sb, i_no, device, block,
			 (index==start_index) ? next_start_index : 0, fblock);
	}

    if (partial)
	buf_write(buf_no);
    else
    {
	buf_release(buf_no);
	b_free(device, iblock);
	i_vect[i_no].i_blocks--;
    }

    return !partial;
}


/*
 * Truncate the file pointed by the specified inode at the given offset.
 */

void
i_truncate(int i_no, unsigned new_offset)
{
    __dev_t device;
    int level, l, block;
    superblock *sb = sb(device = i_vect[i_no].device);
    int fblock = new_offset / sb->s_blksz;


    if (new_offset % sb->s_blksz)
	fblock++;

    set_indirection_level();

    DEBUG(6, "(%x(%x),%d):\n", i_no, i_vect[i_no].dinode, new_offset);

    for (l = level; l < 4; fblock = 0, l++)
	switch (l)
	{
	    case 0:

	      free_dlevel(i_no, fblock);
	      break;

	    case 1:

	      block = get_block_from_inode(i_no, SINGLE_INDIRECT_BLOCK);

	      if (block == 0)
		  break;

	      if (free_ilevel1(sb, i_no, device, block,
			       fblock % BLOCKS_PER_IBLOCK))
		  put_block_to_inode(i_no, SINGLE_INDIRECT_BLOCK, 0);

	      break;

	    case 2:

	      block = get_block_from_inode(i_no, DOUBLE_INDIRECT_BLOCK);

	      if (block == 0)
		  break;

	      if (free_ilevel2(sb, i_no, device, block,
			       fblock / BLOCKS_PER_IBLOCK, fblock))
		  put_block_to_inode(i_no, DOUBLE_INDIRECT_BLOCK, 0);

	      break;

	    case 3:

	      block = get_block_from_inode(i_no, TRIPLE_INDIRECT_BLOCK);

	      if (block == 0)
		  break;

	      free_ilevel3(sb, i_no, device, block,
			   fblock / (BLOCKS_PER_IBLOCK * BLOCKS_PER_IBLOCK),
			   fblock);
	      put_block_to_inode(i_no, TRIPLE_INDIRECT_BLOCK, 0);

	      break;
	}

    i_set_mtime(i_no);

    i_vect[i_no].i_ktime  = seconds;
    i_vect[i_no].i_size   = new_offset;
    i_vect[i_no].modified = 1;
}


/*
 * _bmap() returns in the ibd structure a buffer containing the last level
 * indirect block required in the computation of  the  file  system  block
 * corresponding to the given offset or the inode itself when  the  offset
 * lead to a direct block (the fact that te block number will be  obtained
 * from the inode is specified by a 0 in  the  buf_no  field  of  the  ibd
 * structure.  The _bmap() return value is nonzero if the  indirect  block
 * has been found and 0 otherwise (the file has  a  hole  or  this  is  an
 * attempt to read past the end of file.
 */

int
_bmap(int i_no, unsigned file_offset, iblk_data *ibd)
{
    unsigned block;
    int level, ilevel, buf_no;
    superblock *sb = sb(i_vect[i_no].device);
    int fblock = file_offset / sb->s_blksz;
    unsigned lc[3] = { 1,
		       BLOCKS_PER_IBLOCK,
		       BLOCKS_PER_IBLOCK * BLOCKS_PER_IBLOCK };

    set_indirection_level();

    if (level == 0)
    {
	/* Get the block number from the inode.  */
	ibd->buf_no  = 0;
	ibd->index   = fblock;
	ibd->nblocks = DIRECT_BLOCKS - fblock;
	return 1;
    }

    if ((block = get_block_from_inode(i_no, DIRECT_BLOCKS + level - 1)) == 0)
    {
	/* For bmap() *ONLY*.  */
	ibd->buf_no  = 1;
	ibd->index   = fblock;
	ibd->nblocks = sb->s_blksz / sizeof(unsigned) - ibd->index;
	return 0;
    }

    for (ilevel = 0; ilevel < level - 1; ilevel++)
    {
	if ((buf_no = buf_read(i_vect[i_no].device, block, sb->s_blksz)))
	{
	    block = *(unsigned *)(buf_address(buf_no) +
				  ((fblock / lc[level - 1 - ilevel]) << 2));
	    buf_release(buf_no);
	}
	else
	    block = 0;

	fblock %= lc[level - 1 - ilevel];

	if (block == 0)
	{
	  fail:
	    ibd->buf_no  = 1;   /* for bmap() *ONLY* */
	    ibd->index   = fblock / lc[level - 1 - ilevel];
	    ibd->nblocks = sb->s_blksz / sizeof(unsigned) - ibd->index;
	    return 0;
	}
    }

    buf_no = buf_read(i_vect[i_no].device, block, sb->s_blksz);

    if (buf_no == 0)
	goto fail;

    ibd->buf_no  = buf_no;
    ibd->index   = fblock / lc[level - 1 - ilevel];
    ibd->nblocks = sb->s_blksz / sizeof(unsigned) - ibd->index;

    return 1;
}


/*
 * bmap() returns a pointer to a data area containing the  current  request
 * block numbers.  These numbers are located in the inode or in an indirect
 * block.  The problem here is that we can only return a  pointer  to  that
 * area in the second case, because block numbers are kept into  the  inode
 * as 3 bytes quantities.  We have therefore to create a copy of the  inode
 * direct block area and return a pointer to that copy.
 * The 'i_data'  parameter is a pointer to  an  unsinged  vector  allocated
 * on the stack by the calling function.  The  get_iblk_data() return value
 * is a pointer to the indirect block data area or 'i_data' when the  block
 * numbers are obtained from the inode.  If the  requested  indirect  block
 * does not exist, 'i_zero' is returned.
 * FIXME: using 'i_zero' is a bad idea because it uses an aditional 4 Kb of
 * memory!
 */

unsigned *
bmap(int i_no, unsigned offset, iblk_data *ibd, unsigned *i_data)
{
    int i;

    _bmap(i_no, offset, ibd);

    if (ibd->buf_no == 1)
    {
	ibd->buf_no = 0;
	return i_zero;
    }

    if (ibd->buf_no)
	return (unsigned *)buf_address(ibd->buf_no);

    for (i = ibd->index; i < DIRECT_BLOCKS; i++)
	i_data[i] = get_block_from_inode(i_no, i);

    return i_data;
}


/*
 * rbmap() is called by some higher level functions in the kernel in  order
 * to get a buffer for that part of the file corresponding to the specified
 * offset.  It calls bmap() to get the block number.
 * rbmap() will return 0 if bmap() fails to find a block for the  specified
 * offset (i.e. the file has a hole) or if the given  block  could  not  be
 * read.  Functions calling rbmap will be  unable  to  distinguish  between
 * these two situations and will treat them in the  same  way, filling  the
 * buffer with zeros.  Therefore a bad block will act like a  block  filled
 * with zeros.
 * The new version of  the  file  system  will  only  use  rbmap()  in  the
 * directory parse routines and in __namei() to speed up things.
 *
 * I think this comment is obsolete :-(.
 */

int
rbmap(int i_no, unsigned file_offset)
{
    int block;
    iblk_data ibd;
    unsigned i_data[DIRECT_BLOCKS];
    superblock *sb = sb(i_vect[i_no].device);

    block = bmap(i_no, file_offset, &ibd, i_data)[ibd.index];

    if (ibd.buf_no)
	buf_release(ibd.buf_no);

    if (block)
	return buf_read(i_vect[i_no].device, block, sb->s_blksz);

    return 0;
}


/*
 * Allocate a file system block for an indirect block.  Returns the
 * corresponding buffer.
 */

int
alloc_indirect_block(superblock *sb, int i_no, __dev_t device)
{
    int buf_no = b_alloc(device);

    if (buf_no)
    {
	i_vect[i_no].i_blocks++;
	lmemset(buf_address(buf_no), 0, sb->s_blksz >> 2);
	/* Avoid reading the buffer.  */
	buf_vect[buf_no].valid = 1;
    }

    return buf_no;
}


/*
 * Allocate a file system block for a direct level.  Returns the
 * corresponding buffer.
 */

int
alloc_dlevel(superblock *sb, int i_no, __dev_t device, int index)
{
    int block, buf_no;

    if ((block = get_block_from_inode(i_no, index)))
	return buf_read(device, block, sb->s_blksz);

    if (index < DIRECT_BLOCKS)
    {
	buf_no = b_alloc(i_vect[i_no].device);

	if (buf_no)
	{
	    i_vect[i_no].i_blocks++;
	    lmemset(buf_address(buf_no), 0, sb->s_blksz >> 2);
	    /* Avoid reading the buffer.  */
	    buf_vect[buf_no].valid = 1;
	}
    }
    else
	buf_no = alloc_indirect_block(sb, i_no, device);

    if (buf_no)
	put_block_to_inode(i_no, index, buf_vect[buf_no].block);

    return buf_no;
}


/*
 * Allocate a file system block for an indirect level.  Returns the
 * corresponding buffer.
 */

int
alloc_ilevel(superblock *sb, int i_no, __dev_t device,
	     int buf_no, int index, int last)
{
    int block, sbuf_no;

    if ((block = *(unsigned *)(buf_address(buf_no) + (index << 2))))
    {
	buf_release(buf_no);
	return buf_read(device, block, sb->s_blksz);
    }

    if (last)
    {
	sbuf_no = b_alloc(device);

	if (sbuf_no)
	{
	    i_vect[i_no].i_blocks++;
	    lmemset(buf_address(sbuf_no), 0, sb->s_blksz >> 2);
	    /* Avoid reading the buffer.  */
	    buf_vect[sbuf_no].valid = 1;
	}
    }
    else
	sbuf_no = alloc_indirect_block(sb, i_no, device);

    if (sbuf_no)
	*(unsigned *)(buf_address(buf_no) + (index << 2)) =
	    buf_vect[sbuf_no].block;

    buf_write(buf_no);
    return sbuf_no;
}


/*
 * wbmap() returns a buffer corresponding to the 'file_offset' possition
 * in the file.  If there are no more free file system blocks it returns
 * 0.
 */

int
wbmap(int i_no, unsigned file_offset)
{
    superblock *sb = sb(i_vect[i_no].device);
    int fblock = file_offset / sb->s_blksz;
    int level, buf_no, device = i_vect[i_no].device;

    set_indirection_level();

    switch (level)
    {
	case 0:

	  return alloc_dlevel(sb, i_no, device, fblock);

	case 1:

	  buf_no = alloc_dlevel(sb, i_no, device, SINGLE_INDIRECT_BLOCK);

	  if (buf_no == 0)
	      return 0;

	  return alloc_ilevel(sb, i_no, device, buf_no, fblock, 1);

	case 2:

	  buf_no = alloc_dlevel(sb, i_no, device, DOUBLE_INDIRECT_BLOCK);

	  if (buf_no == 0)
	      return 0;

	  buf_no = alloc_ilevel(sb, i_no, device, buf_no,
				fblock/BLOCKS_PER_IBLOCK,0);

	  if (buf_no == 0)
	      return 0;

	  return alloc_ilevel(sb, i_no, device, buf_no,
			      fblock % BLOCKS_PER_IBLOCK, 1);

	case 3:

	  buf_no = alloc_dlevel(sb, i_no, device, TRIPLE_INDIRECT_BLOCK);

	  if (buf_no == 0)
	      return 0;

	  buf_no = alloc_ilevel(sb, i_no, device, buf_no,
				fblock / (BLOCKS_PER_IBLOCK*BLOCKS_PER_IBLOCK),
				0);

	  if (buf_no == 0)
	      return 0;

	  buf_no = alloc_ilevel(sb, i_no, device, buf_no,
				fblock / BLOCKS_PER_IBLOCK, 0);

	  if (buf_no == 0)
	      return 0;

	  return alloc_ilevel(sb, i_no, device, buf_no,
			      fblock % BLOCKS_PER_IBLOCK, 1);

	default:

	  PANIC("%s %x", ERR_BADLVL, level);
    }

    return 0;
}


/*
 * Parse a path string removing any additional (redundant) slashes and
 * dots.  That is, a path like /usr/local/././///bin will be processed
 * and returned as /usr/local/bin.
 */

char *
clear_path(char *path)
{
    char *cpath = path;
    char *opath = path;

    if (*opath == 0)
	return path;

    if (*opath == '/')
	*cpath++ = *opath++;

    while (*opath)
    {
	while (*opath == '/' ||
	       (*opath == '.' && (*(opath + 1) == '/' || *(opath + 1) == 0)))
	    opath++;

	while (*opath && *opath != '/')
	    *cpath++ = *opath++;

	if (*opath)
	    *cpath++ = '/';
    }

    if (*(cpath - 1) == '/' && cpath - path > 1)
	cpath--;

    *cpath = 0;
    return path;
}


/*
 * The classical `name to inode' algorithm front end.
 *
 * Arguments:
 * - path is the path to the file whose inode we need.
 *
 * Returned values:
 * - the return value is the file's inode.
 * - *empty_dir_slot is an empty slot in the last directory in the path.
 *   Since we parse the path anyway, we can return it without too much
 *   effort, for sys_mkdir() to use it.
 * - *dir_i_no is the directory's inode
 * - *f_offset is the offset of the file name in its parent directory.
 * - **f_name is the file (or directory) name, that is, the name of the
 *   last path element.
 */

int
__namei(char *_path, int *empty_dir_slot, int *dir_i_no,
	int *f_offset, char **f_name)
{
    superblock *sb;
    unsigned address = 0;
    int i_no, cnt, buf_no = 0, device, offset, path_err;
    char name[NAME_MAX + 1], kpath[PATH_MAX + 1], *path = kpath;


    *empty_dir_slot = -1;
    *f_offset       = -1;
    *dir_i_no       =  0;

    if ((path_err = path_bad(_path)))
	return path_err;

    /* clear_path() might modify the string.  */
    strcpy(kpath, _path);

    DEBUG(6, "(%s)\n", path);

    /* Clean up the path before proceeding.  */
    if (clear_path(path)[0] == '/')
    {
	/* We are dealing with an absolute path.  */
	*f_name = path + 1;

	/* FIXME: not sure about this one...  */
	if (i_vect[root_inode].i_nlinks == 0)
	    PANIC(ERR_BADROOT);

	i_duplicate(i_no = root_inode);

	/* Return the inode just found if this is the root directory.  */
	if (path[1] == '\0')
	    return i_no;
    }
    else
    {
	/* We are dealing with a relative path.  */
	if (i_vect[this->cwd_inode].i_nlinks == 0)
	    return -ENOENT;

	i_duplicate(i_no = this->cwd_inode);
    }

    if (*path == 0)
	return *f_name = path, i_no;

  next_path:

    /* Get the superblock pointer.  */
    sb = sb(device = i_vect[i_no].device);

    while (*path)
    {
	if (*path == '/')
	    path++;

	*f_name = path;

	/* Search the end of this path element.  A path element will end at
	   the next '/' or at the end of the string.  */
	for (cnt = 0;
	     *path && *path != '/' && cnt < sb->s_direntsz - sizeof(__ino_t);
	     name[cnt++] = *path++)
	    ;

	if (*path != '/' && *path != '\0')
	{
	    /* The current path element is too long.  I should
               truncate it, bu I guess I am too lazy to do it.  */
	    i_get_rid_of(i_no);
	    return -ENAMETOOLONG;
	}

	name[cnt++] = 0;

	/* i_no should point to a directory.  */
	if (!S_ISDIR(i_vect[i_no].i_mode))
	{
	    i_get_rid_of(i_no);
	    return -ENOTDIR;
	}

	if (!permitted(i_no, X_OK, this->access_flag))
	{
	    i_get_rid_of(i_no);
	    return -EACCES;
	}

	/* The ".." directory in "/" refers to "/" itself.  It is the
           root inode of the file system.  */
	if (device == root_device &&
	    i_no   == root_inode  &&
	    name[0] == '.' && name[1] == '.' && name[2] == '\0')
	    continue;

	/* If *path is '\0', it means we have reached the end of the
           path and therefore the last inode found is the inode of the
           last path element that is a directory.  The only thing that
           remains to be done is to parse that directory in the search
           of an entry matching the last path element.  */
	if (*path == '\0')
	    *dir_i_no = i_no;

	/* Parse the entries in the directory, searching for an entry
           matching the last path element.  */
	for (offset = 0, *empty_dir_slot = -1;
	     offset < i_vect[i_no].i_size;
	     offset += sb->s_direntsz)
	{
	    if (offset % sb->s_blksz == 0)
	    {
		/* Directories cannot contain holes.  In theory, when
		   deleting directory entries in dir.c we can figure
		   out if a block is empty, (i.e. one that no longer
		   contains valid directory entries), and deallocate
		   the it.  However, this case is not frequent and I
		   don't want to make the implementation slower just
		   for a small improvement in terms of free file
		   system blocks.  */
		if ((buf_no = rbmap(i_no, offset)) == 0)
		    PANIC(ERR_HOLEINDIR);

		address = (unsigned)buf_address(buf_no);
	    }

	    /* If we have found an empty directory entry, save it for later.
	       We will return it to the caller, so that it can fill it.  */
	    if (*(__ino_t *)(address + offset % sb->s_blksz) == 0)
	    {
		if (*empty_dir_slot == -1)
		    *empty_dir_slot = offset;

		goto release_buffer;
	    }

	    /* Compare the current directory entry with the file name.  */
	    if (memcmp((char*)(address + offset%sb->s_blksz + sizeof(__ino_t)),
		       name, min(cnt, sb->s_direntsz - sizeof(__ino_t))) == 0)
	    {
		/* *path == '\0' means we are at the end of the path,
                   i.e. we have found the latest path element, so we
                   save its directory offset.  */
		if (*path == '\0')
		    *f_offset = offset;

		/* If i_no is the root inode of the file system (NOTE:
		   not the root inode of the root file system, as that
		   case has been handled before), we will cross a
		   mount point.  Otherwise, we simply get an in core
		   inode for the directory entry's inode.  */
		if (i_vect[i_no].dinode == ROOT_INODE &&
		    (name[0] == '.' && name[1] == '.' && name[2] == '\0'))
		{
		    int mp = find_mp(0, i_no);

		    if (*path)
			i_get_rid_of(i_no);

		    i_no = mpt[mp].dir_i_no;
		    i_lock(i_no);
		    i_duplicate(i_no);
		}
		else
		{
		    if (*path)
			i_get_rid_of(i_no);

		    i_no = i_get(device,
				 *(__ino_t *)(address + offset % sb->s_blksz));
		}

		if (i_vect[i_no].i_nlinks == 0)
		{
		    /* RACE: trying to access a removed inode.  */
		    DEBUG(6, "race: trying to access removed inode %x(%x).\n",
			  i_no, i_vect[i_no].dinode);
		    i_get_rid_of(i_no);
		    buf_release(buf_no);
		    return -ENOENT;
		}

		/* Since we got the current path element's inode, we
                   no longer need the buffer, as we will continue the
                   search in another directory (if any).  */
		buf_release(buf_no), buf_no = 0;
		goto next_path;
	    }

	  release_buffer:

	    /* If this is the last directory entry in this block, we
               can release the buffer.  */
	    if (offset % sb->s_blksz == sb->s_blksz - sb->s_direntsz)
		buf_release(buf_no), buf_no = 0;
	}

	/* Release the buffer, if not already released (buf_no == 0).  */
	if (buf_no)
	    buf_release(buf_no);

	
	if (*path)
	{
	    /* We couldn't find a matching entry.  */
	    i_get_rid_of(i_no);
	    return -ENOENT;
	}

	*dir_i_no = i_no;
	return 0;
    }

    return i_no;
}


/*
 * __namei() back end.  Called by routines that are not interested in
 * the directory where the requested entry resides.  Get only the
 * inode corresponding to `path'.
 */

int
namei(char *path)
{
    char *name;
    int i_no, empty_dir_slot, dir_i_no, dir_offset;


    i_no = __namei(path, &empty_dir_slot, &dir_i_no, &dir_offset, &name);

    if (dir_i_no)
	i_get_rid_of(dir_i_no);

    return (i_no == 0) ? -ENOENT : i_no;
}


/*
 * Another __namei() back end.  This should only be called when 'path' cannot
 * be the root directory.  Called by sys_mount().
 */

int
namei_with_dir(char *path, int *dir_i_no)
{
    char *name;
    int i_no, empty_dir_slot, dir_offset;


    i_no = __namei(path, &empty_dir_slot, dir_i_no, &dir_offset, &name);

    if (i_no <= 0)
    {
	if (*dir_i_no)
	    i_get_rid_of(*dir_i_no);

	return (i_no == 0) ? -ENOENT : i_no;
    }

    if (*dir_i_no == 0)
    {
	/* It seems that i_no points to the root directory.  That
	   directory cannot be used as a mount point, so just report
	   EBUSY.  */
	i_get_rid_of(i_no);
	return -EBUSY;
    }

    return i_no;
}


/*
 * Clean up the inode-related data structures.  Return the number of errors
 * encountered.  Actually this only performs some consistency checkings.
 */

int
i_cleanup(void)
{
    int i;
    int errors = 0;

    /* Check for used inodes.  */
    for (i = 0; i < MAX_INODES; i++)
	if (i_vect[i].nref)
	{
	    printk("WARNING: inode %x(%x,%x) still in use (count = %x).\n",
		   i, i_vect[i].device, i_vect[i].dinode, i_vect[i].nref);
	    errors++;
	}

    return errors;
}


/*
 * Initialize the inode management data structures.
 */

void
i_init(void)
{
    int i;
    int size = (MAX_INODES + INODES_BUCKETS_NO) * sizeof(inode_t);

    /* Allocate space for buf_vect[].  */
    i_vect = (inode_t *)vm_static_alloc(size, size, sizeof(inode_t *));

    memset(i_vect, 0, size);
    memset(i_zero, 0, sizeof(i_zero));

    for (i = 0; i < MAX_INODES; i_vect[i++].hnext = 0xFFFF);

    i_vect[0].lnext = i_vect[0].lprev = 0;

    for (i = 1; i < MAX_INODES; i_list_insert(i++));

    for (i = 0; i < INODES_BUCKETS_NO; i++)
	i_vect[MAX_INODES + i].hnext =
	i_vect[MAX_INODES + i].hprev = MAX_INODES + i;
}
