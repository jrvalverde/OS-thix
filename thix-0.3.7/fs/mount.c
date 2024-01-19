/* mount.c -- Mounts & superblocks management.  */

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
#include <thix/vmalloc.h>
#include <thix/string.h>
#include <thix/sleep.h>
#include <thix/sched.h>
#include <thix/process.h>
#include <thix/buffer.h>
#include <thix/vmdev.h>
#include <thix/fs.h>
#include <thix/stat.h>
#include <thix/gendrv.h>
#include <thix/fcntl.h>
#include <thix/fdc.h>
#include <thix/hdc.h>
#include <thix/cluster.h>
#include <thix/generic.h>
#include <thix/device.h>


static semaphore sync_mutex  = 1;
static semaphore mount_mutex = 1;


/* `root_device' should contain the major/minor numbers of the root
    file system.  Take a look at driver.c for details on computing
    this number.  */

/* /dev/hda3  */
int root_device = HDC_MAJOR * 0x100 + 4;

#if 0
/* /dev/fd0  */
int root_device = FDC_MAJOR * 0x100 + 0;
#endif

int root_inode;


/*
 * This is the mount points table.
 */

mp_struct mpt[SYSTEM_MOUNT_MAX];


/* superblock errors. */
#define SBE_OK           0
#define SBE_CHECKSUM    -1
#define SBE_MAGIC       -2
#define SBE_BLKSZ       -3
#define SBE_DIRENTSZ    -4


/* Superblock warnings.  */
#define SBW_BBLSZ       sb_warning[0]
#define SBW_TINODE      sb_warning[1]
#define SBW_TFREE       sb_warning[2]
#define SBW_STRUCTURE   sb_warning[3]


static char *sb_warning[] =
{
    "bad block list size",
    "free inodes number",
    "free blocks number",
    "file system structure",
};


static char SB_WARNING[] = "TFS: warning: invalid %s in superblock\n";


/*
 * Mount point entries allocation.  A mount point entry is free if it's
 * sb field is NULL.
 */

int
get_mp(__dev_t device)
{
    int mp;     /* mount point table index. */

    /* Is the device already mounted ?  */
    for (mp = 1; mp < SYSTEM_MOUNT_MAX; mp++)
	if (mpt[mp].sb && (mpt[mp].device == device))
	{
	    DEBUG(6, "device %w already mounted\n", device);
	    return 0;
	}

    for (mp = 1; mp < SYSTEM_MOUNT_MAX; mp++)
	if (mpt[mp].sb == NULL)
	{
	    /* Prevent reallocation.  */
	    mpt[mp].sb = (superblock *)1;
	    mpt[mp].device   = device;
	    mpt[mp].icluster = -1;
	    mpt[mp].bcluster = -1;
	    mpt[mp].icount   = 0;
	    mpt[mp].bcount   = 0;
	    return mp;
	}

    return 0;
}


/*
 * Release a mount point.
 */

void
release_mp(int mp)
{
#ifdef __PARANOIA__
    if (mp < 1 || mp >= SYSTEM_MOUNT_MAX)
	PANIC("releasing invalid mount point (%d)", mp);
#endif

    mpt[mp].sb = NULL;
}


/*
 * Find an available mount point.
 */

int
find_mp(int mp_i_no, int root_i_no)
{
    int mp;     /* Mount point table index. */

    if (mp_i_no)
    {
	for (mp = 1; mp < SYSTEM_MOUNT_MAX; mp++)
	    if (mpt[mp].sb && mpt[mp].mp_i_no == mp_i_no)
		return mp;
    }
    else if (root_i_no)
    {
	for (mp = 1; mp < SYSTEM_MOUNT_MAX; mp++)
	    if (mpt[mp].sb && mpt[mp].root_i_no == root_i_no)
		return mp;
    }

    PANIC("(%x(%x),%x(%x)) can't find mount point\n",
	  mp_i_no, i_vect[mp_i_no].dinode,
	  root_i_no, i_vect[root_i_no].dinode);

    /* Not reached.  */
    return -1;
}


/*
 * Check the superblock.
 */

static int
check_superblock(superblock *sb)
{
    unsigned s;                 /* Superblock checksum.  */
    int sb_warnings = 0;        /* Superblock warnings count.  */
    unsigned total_inodes;      /* Total number of inodes in the fs.  */
    unsigned total_blocks;      /* Total number of data blocks in the fs.  */
    unsigned total_fs_blocks;   /* Total number of blocks in the fs.  */


    s = checksum((unsigned char *)sb,
		 sizeof(superblock) - sizeof(sb->s_checksum));

    if (s != sb->s_checksum)
	return SBE_CHECKSUM;

    if (sb->s_magic != TFS_MAGIC_V02)
	return SBE_MAGIC;

    if (sb->s_blksz != 0x200 &&
	sb->s_blksz != 0x400 &&
	sb->s_blksz != 0x800)
	return SBE_BLKSZ;

    if (sb->s_direntsz != 0x10 &&
	sb->s_direntsz != 0x20 &&
	sb->s_direntsz != 0x40 &&
	sb->s_direntsz != 0x80 &&
	sb->s_direntsz != 0x100)
	return SBE_DIRENTSZ;

    /* FIXME: s_fsize should be checked using an ioctl() to get the device
       size.  */

    /* Ok, no fatal errors since here.  Let's issue some warnings...  */

    total_inodes = sb->s_clusters * sb->s_icluster;
    total_blocks = (sb->s_clusters - 1) * sb->s_bcluster +
		   (sb->s_lcflag ? sb->s_blcluster : sb->s_bcluster);

    if (sb->s_bblsz > (TFS_MAX_BAD_BLOCKS * sizeof(unsigned)) / sb->s_blksz)
	printk(SB_WARNING, SBW_BBLSZ), sb_warnings++;

    if (sb->s_ifree > total_inodes)
	printk(SB_WARNING, SBW_TINODE), sb_warnings++;

    if (sb->s_bfree > total_blocks)
	printk(SB_WARNING, SBW_TFREE), sb_warnings++;

    total_fs_blocks = 1 + 1 + sb->s_ssbldsz + sb->s_bblsz +
		      (1+1+1+(sb->s_icluster*sizeof(dinode_t))/sb->s_blksz) *
		      sb->s_clusters +
		      sb->s_bcluster * (sb->s_clusters - 1) +
		      (sb->s_lcflag ? sb->s_blcluster : sb->s_bcluster) +
		      sb->s_ublocks;

    if (total_fs_blocks != sb->s_fsize)
	printk(SB_WARNING, SBW_STRUCTURE), sb_warnings++;

    if (!sb->s_flock || !sb->s_ilock)
    {
	printk("TFS: warning: the superblock was not locked when");
	printk("last updated\n");
    }

    if (sb->s_fmod || sb->s_state != TFS_MAGIC_V02)
    {
	printk("TFS: warning: recovering after a bad shutdown\n");
	sb_warnings++;
    }

    if (sb_warnings)
	printk("TFS: mounting a file system with errors, please run tfsck.\n");

    sb->s_flock = 0;
    sb->s_ilock = 0;

    return sb_warnings;
}


int
get_superblock(__dev_t device)
{
    int mp;             /* Mount point table index.  */
    int blksz;          /* Block size.  */
    int result;         /* Check_superblock() result.  */
    int bb_buf_no;      /* Boot block buffer number.  */
    int sb_buf_no;      /* Superblock buffer number.  */


    mp = mp(device);

    DEBUG(6, "mp(%x)=%x\n", device, mp);

    bb_buf_no = buf_read(device, TFS_BOOT_BLOCK, 1024);
    blksz = buf_address(bb_buf_no)[0x1fd] * 512;

    /* Get rid of it.  */
    buf_vect[bb_buf_no].valid = 0;
    buf_release(bb_buf_no);

    if (blksz != 0x200 &&
	blksz != 0x400 &&
	blksz != 0x800)
    {
	printk("TFS: unable to detect file system block size.\n"
	       "TFS: cannot load superblock.\n");
	return 0;
    }

    sb_buf_no = buf_read(device, TFS_SUPERBLOCK, blksz);

    if (sb_buf_no == 0)
    {
	printk("TFS: invalid superblock\n");
	return 0;
    }

    result = check_superblock((superblock *)buf_address(sb_buf_no));

    if (result < 0)
    {
	printk("TFS: superblock error %x\n", result);
	buf_release(sb_buf_no);
	return 0;
    }

    mpt[mp].sb = (superblock *)vm_xalloc(sizeof(superblock));
    memcpy(mpt[mp].sb, buf_address(sb_buf_no), sizeof(superblock));

    buf_release(sb_buf_no);

    return 1;
}


/*
 * update_superblock() writes the superblock back to  the  file system  in
 * its normal place (block 1) and updates all its backups (at the begining
 * of each cluster).
 */

void
update_superblock(__dev_t device)
{
    int sb_buf_no;              /* Superblock buffer number.  */
    int cluster = -1;           /* Cluster number.  */
    superblock *sb;             /* Pointer to the device superblock.  */


    sb = sb(device);

    while (sb->s_flock || sb->s_ilock)
    {
	DEBUG(6, "the superblock is locked...\n");
	sleep(sb, WAIT_SUPERBLOCK);
    }

    sb->s_flock = 1;
    sb->s_ilock = 1;
    sb->s_fmod  = 0;

    sb->s_time = seconds;

    sb->s_checksum = checksum((unsigned char *)sb,
			      sizeof(superblock) - sizeof(sb->s_checksum));

    sb_buf_no = buf_get(device, TFS_SUPERBLOCK, sb->s_blksz);
    goto do_write_superblock;

    for (; cluster < (int)sb->s_clusters; cluster++)
    {
	sb_buf_no = buf_get(device, cluster_reserved2fs(cluster, 0),
			    sb->s_blksz);
      do_write_superblock:
	memcpy(buf_address(sb_buf_no), sb, sizeof(superblock));
	memset(buf_address(sb_buf_no) + sizeof(superblock), 0,
	       sb->s_blksz - sizeof(superblock));
	buf_write(sb_buf_no);
    }

    sb->s_flock = 0;
    sb->s_ilock = 0;
    wakeup(sb);
}


/*
 * Release a superblock.
 */

void
release_superblock(__dev_t device)
{
    int mp = mp(device);
    update_superblock(device);

    vm_xfree(mpt[mp].sb);
    mpt[mp].sb = NULL;
}


/*
 * Check if the file system is currently used.  This function doesn't
 * belong here.  It belongs in inode.c.
 */

int
fs_in_use(__dev_t device)
{
    int index, i_no, next;

    for (index = MAX_INODES; index < MAX_INODES + INODES_BUCKETS_NO; index++)
	for (i_no = i_vect[index].hnext; i_no != index; i_no = next)
	{
	    /* i_hash_delete() makes the hnext pointer invalid.  */
	    next = i_vect[i_no].hnext;

	    if (i_vect[i_no].device == device)
	    {
		switch (i_vect[i_no].nref)
		{
		    case 0:
			/* This should be replaced with i_hash_delete().  */
			i_remove(i_no);
			continue;

		    case 1:
			if (i_vect[i_no].dinode == ROOT_INODE)
			{
			    /* We will remove this one later.  */
			    continue;
			}

		    default:
			/* We have found an in use inode.  Give up.  */
			return 1;
		}
	    }
	}

    return 0;
}


/*
 * The mount() system call.
 */

int
sys_mount(char *device_name, char *directory, int options)
{
    int mp;             /* Mount point table index.  */
    int vmdev;          /* VM device number.  */
    int mp_i_no;        /* The mount point inode.  */
    __dev_t device;     /* Major/minor device numbers of the fs to mount.  */
    int dev_i_no;       /* Special file inode.  */
    int root_i_no;      /* Root inode of the file system to mount.  */
    int dir_i_no;
    int result;


    DEBUG(4, "(%s,%s,%d)\n", device_name, directory, options);

    if (!superuser())
	return -EPERM;

    dev_i_no = namei(device_name);

    if (dev_i_no < 0)
	return dev_i_no;

    if (!S_ISBLK(i_vect[dev_i_no].i_mode))
    {
	i_get_rid_of(dev_i_no);
	return -ENOTBLK;
    }

    device = i_vect[dev_i_no].i_rdev;

    i_get_rid_of(dev_i_no);


    mp_i_no = namei_with_dir(directory, &dir_i_no);

    if (mp_i_no < 0)
	return mp_i_no;

    if (!S_ISDIR(i_vect[mp_i_no].i_mode))
    {
	i_get_rid_of2(dir_i_no, mp_i_no);
	return -ENOTDIR;
    }

    if (i_vect[mp_i_no].nref > 1)
    {
	i_get_rid_of2(dir_i_no, mp_i_no);
	return -EBUSY;
    }

    drv_open(device, O_RDWR);

    mp = get_mp(device);

    if (mp == 0)
    {
	drv_close(device);
	i_get_rid_of2(dir_i_no, mp_i_no);
	return -EBUSY;
    }

    set_minor_info(device, mp);

    result = get_superblock(device);

    if (result == 0)
    {
      fail:
	release_mp(mp);
	drv_close(device);
	i_get_rid_of2(dir_i_no, mp_i_no);
	return -EINVAL;
    }

    vmdev = get_vmdev(device, FS_DEVICE, 0);

    if (vmdev == -1)
	goto fail;


    /* Keep this code atomic to prevent races.  */
    down(&mount_mutex, WAIT_DEVICE_BUSY);

    root_i_no = i_get(device, ROOT_INODE);

    i_vect[mp_i_no].mount_point = 1;

    mpt[mp].vmdev     = vmdev;
    mpt[mp].dir_i_no  = dir_i_no;
    mpt[mp].mp_i_no   = mp_i_no;
    mpt[mp].root_i_no = root_i_no;
    mpt[mp].ibitmap   = (unsigned *)vm_xalloc(mpt[mp].sb->s_blksz);
    mpt[mp].bbitmap   = (unsigned *)vm_xalloc(mpt[mp].sb->s_blksz);

    i_unlock(dir_i_no);
    i_unlock(mp_i_no);
    i_unlock(root_i_no);

    up(&mount_mutex);


    DEBUG(6, "mp=%z mp_i_no=%x(%x) root_i_no=%x(%x) device=%w\n",
	  mp, mp_i_no, i_vect[mp_i_no].dinode, root_i_no,
	  i_vect[root_i_no].dinode, device);

    return 0;
}


/*
 * The umount() system call.
 */

int
sys_umount(char *device_name)
{
    int mp, device, i_no;


    DEBUG(4, "(%s)\n", device_name);

    if (!superuser())
	return -EPERM;

    i_no = namei(device_name);

    if (i_no < 0)
	return i_no;

    if (!S_ISBLK(i_vect[i_no].i_mode))
    {
	i_get_rid_of(i_no);
	return -ENOTBLK;
    }

    device = i_vect[i_no].i_rdev;

    i_get_rid_of(i_no);

    if (check_device(device) == 0)
	return -EINVAL;

    mp = mp(device);

    if (!mounted(device))
	return -EINVAL;

    /* If fs_in_use() returns 1 (which mean we cannot umount the file
       system), some inodes will be unnecessary deleted from the inode
       cache.  */
    if (fs_in_use(device))
	return -EBUSY;

    /* Keep this code atomic to prevent races.  */
    down(&mount_mutex, WAIT_DEVICE_BUSY);

    i_vect[mpt[mp].mp_i_no].mount_point = 0;

    release_vmdev(mpt[mp].vmdev);

    i_get_rid_of(mpt[mp].dir_i_no);
    i_get_rid_of(mpt[mp].mp_i_no);
    i_get_rid_of(mpt[mp].root_i_no);


    /* Remove the root inode of the file system to be unmounted from
       the inode cache.  */
    i_remove(mpt[mp].root_i_no);

    flush_cluster(mp, TFS_INODES_BITMAP);
    flush_cluster(mp, TFS_BLOCKS_BITMAP);
    release_superblock(device);

    vm_xfree(mpt[mp].ibitmap);
    vm_xfree(mpt[mp].bbitmap);

    release_mp(mp);

    if (can_close_device(device, -1))
    {
	buf_sync(device, 1);
	drv_close(device);
    }
    else
	buf_sync(device, 0);

    up(&mount_mutex);

    return 0;
}


/*
 * Mount the root file system.
 */

int
mount_root(void)
{
    int mp;                     /* Mount point table index.  */
    int vmdev;                  /* VM device number.  */
    superblock *sb;             /* Superblock pointer.  */
    int result;


    printk("TFS: mounting root [%w]...(r/w)\n", root_device);

    if (sizeof(superblock) != 512)
	PANIC("\nINVALID sizeof(superblock) = %d!\n"
	      "Adjust it and recompile the kernel.\n", sizeof(superblock));

    vmdev = get_vmdev(root_device, FS_DEVICE, 0);

    drv_open(root_device, O_RDWR);

    if ((mp = get_mp(root_device)) == 0)
	PANIC("\nTFS: can't get a mount point entry\n");

    set_minor_info(root_device, mp);

    result = get_superblock(root_device);

    if (result == 0)
	PANIC("\nTFS: can't get root superblock\n");

    sb = mpt[mp].sb;

    mpt[mp].vmdev   = vmdev;
    mpt[mp].ibitmap = (unsigned *)vm_xalloc(sb->s_blksz);
    mpt[mp].bbitmap = (unsigned *)vm_xalloc(sb->s_blksz);

    printk("TFS: file system type: %s\n", sb->s_ftype);
    printk("TFS: file system name: %s\n", sb->s_fname);
    printk("TFS: free file system space: %dk\n",
	   sb->s_bfree * sb->s_blksz / 1024);

    root_inode = i_get(root_device, ROOT_INODE);
    i_unlock(root_inode);

    return 1;
}


/*
 * Unmount the root file system.
 */

void
umount_root(void)
{
    int mp;             /* Mount point table index.  */

    /* Note that the root inode becomes the cwd_inode of init in
       init.c so there is no need to deallocate it here.  FIXME: is
       this the right way to do it ?  */

    DEBUG(4, "\n");

    mp = mp(root_device);

    vm_xfree(mpt[mp].ibitmap);
    vm_xfree(mpt[mp].bbitmap);

    release_superblock(mpt[mp].device);
    buf_sync(mpt[mp].device, 1);
    drv_close(mpt[mp].device);
    release_vmdev(mpt[mp].vmdev);
    release_mp(mp);
}


/*
 * The sync() system call.
 */

int
sys_sync(void)
{
    int mp;


    DEBUG(4, "\n");

    if (sync_mutex == 0)
	return 0;

    sync_mutex = 0;

    for (mp = 0; mp < SYSTEM_MOUNT_MAX; mp++)
	if (mpt[mp].sb)
	{
	    update_superblock(mpt[mp].device);
	    flush_cluster(mp, TFS_INODES_BITMAP);
	    flush_cluster(mp, TFS_BLOCKS_BITMAP);
	}

    buf_sync(SYNC_ALL_DEVICES, 0);

    sync_mutex = 1;
    return 0;
}


/*
 * Initialize the mount tables.
 */

void
mount_init(void)
{
    lmemset(mpt, 0, sizeof(mpt) >> 2);
}
