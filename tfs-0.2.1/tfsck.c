
/*
 *
 *	Thix Operating System
 *	Copyright (c) 1993,1994,1995 Tudor Hulubei
 *
 *	tfsck.c
 *	Thix File System (TFS) check utility.
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <termios.h>


/* Just temporary.  */
#include "../thix/include/thix/fs.h"
#include "../thix/include/thix/cluster.h"
#include "../thix/include/thix/inode.h"


/* Just temporary.  */
#define THIX_PARTITION_NAME "/dev/hda3"
#define THIX_PARTITION_SIZE 66528


#define ERR_NOT_SUPERUSER	   1
#define ERR_USAGE_REQUEST	   2
#define ERR_UNKNOWN_OPTION	   3
#define ERR_UNKNOWN_ERROR	   4
#define ERR_NO_DEVICE		   5
#define ERR_BAD_BLOCK_SIZE	   6
#define ERR_UNKNOWN_DEVICE	   7
#define ERR_OPEN_DEVICE		   8
#define ERR_IO_ERROR		   9
#define ERR_NEED_BLOCK_DEVICE	  10
#define ERR_TOO_MANY_INODES	  11
#define ERR_TOO_FEW_INODES	  12
#define ERR_BAD_BAD_BLOCKS_NO	  13
#define ERR_DIR_ENTRY_SIZE	  14
#define ERR_INVALID_BBFS_BLKSZ	  15
#define ERR_INVALID_SUPERBLOCK	  16
#define ERR_NOT_ENOUGH_MEMORY	  17
#define ERR_STAGE1_ERRORS	  18
#define ERR_R_A			  19
#define ERR_NO_TTY		  20

#define fs_inode	(cluster * sb->s_icluster + inode)


#define TTY_OUTPUT 1


static struct termios old_term;
static struct termios new_term;


/* ugly, but I can use some macros in <thix/fs.h> */
superblock tfs_superblock;
superblock *sb = &tfs_superblock;

char *program;			/* this program name.  */
int verbose = 0;		/* tfsck should be verbose.  */
int repair = 0;			/* ask for confirmations before repairing.  */
int auto_repair = 0;		/* repairs without confirmations.  */
int inodes_info = 0;		/* tfsck should display aditional informations
				   about inodes with holes, blocks past EOF. */
int handle;			/* file handle of the device to check.  */
int cluster;			/* currently checked cluster number.  */
int inode;			/* currently checked inode number. Note
				   that this number is cluster relative.  */
int size;			/* size of the file pointed by the current
				   checked inode.  */
int total_free_inodes = 0;	/* total free inodes on the device to check. */
int total_free_blocks = 0;	/* total free blocks on the device to check. */
unsigned *ibitmap;		/* buffer for cluster inode bitmaps.  */
unsigned *bbitmap;		/* buffer for cluster block bitmaps.  */
dinode *cluster_inodes;		/* buffer for cluster inodes.  */
unsigned *si_blockp;		/* buffer for the single indirect level.  */
unsigned *di_blockp;		/* buffer for the double indirect level.  */
unsigned *ti_blockp;		/* buffer for the triple indirect level.  */
unsigned si_block;		/* single indirect block number.  */
unsigned di_block;		/* double indirect block number.  */
unsigned ti_block;		/* triple indirect block number.  */
unsigned inode_data_blocks;	/* data blocks in the current inode.  */
unsigned inode_total_blocks;	/* total blocks in the current inode.  */
unsigned *bitmap;		/* file system blocks bitmap.  */
unsigned short *i_nlinks;	/* array containing the number of links of
				   each inode.  */
int fs_size;			/* file system size in number of blocks.  */
int fs_blksz;			/* file system block size.  */
int fs_errors = 0;		/* file system errors.  */
int sb_errors = 0;		/* superblock errors.  */


typedef struct
{
    unsigned inode;
    char name[1];
} tfs_dirent;


#define MSG_INVALID_TFS_MAGIC		message[ 0]
#define MSG_INVALID_CHECKSUM		message[ 1]
#define MSG_INVALID_BLKSZ		message[ 2]
#define MSG_INVALID_DIRENTSZ		message[ 3]
#define MSG_INVALID_BBLSZ		message[ 4]
#define MSG_INVALID_IFREE		message[ 5]
#define MSG_INVALID_BFREE		message[ 6]
#define MSG_INVALID_FSIZE		message[ 7]
#define MSG_INVALID_CFSIZE		message[ 8]
#define MSG_INVALID_SHUTDOWN		message[ 9]
#define MSG_OUT_OF_RANGE		message[10]
#define MSG_BLOCK_PAST_EOF		message[11]
#define MSG_DUPLICATE_BLOCK		message[12]
#define MSG_RESERVED_BLOCK		message[13]
#define MSG_FREE_INODE_HAS_MODE		message[14]
#define MSG_FREE_INODE_HAS_NLINKS	message[15]
#define MSG_BLOCK_MARKED_FREE		message[16]
#define MSG_BLOCK_MARKED_USED		message[17]
#define MSG_INVALID_INODE_SIZE		message[18]
#define MSG_INVALID_INODE_NLINKS_0	message[19]
#define MSG_INVALID_INODE_MODE		message[20]
#define MSG_NO_DOT_ENTRY		message[21]
#define MSG_NO_DOTDOT_ENTRY		message[22]
#define MSG_INVALID_DOT_ENTRY		message[23]
#define MSG_INVALID_DOTDOT_ENTRY	message[24]
#define MSG_INVALID_INODE_NUMBER	message[25]
#define MSG_ENTRY_NOT_DIR		message[26]
#define MSG_INVALID_INODE_NLINKS	message[27]
#define MSG_SUPERBLOCK_NOT_LOCKED	message[28]
#define MSG_INVALID_DI_BLOCKS		message[29]
#define MSG_INODE_USE_BLOCKS		message[30]
#define MSG_INVALID_BLOCKS_BITMAP	message[31]
#define MSG_INVALID_INODES_BITMAP	message[32]


char *message[] =
{
    "invalid file system magic number: %x (expecting %x)\n",
    "invalid superblock checksum: %x (expecting %x)\n",
    "invalid file system block size: %x (expecting 512,1024 or 2048)\n",
    "invalid directory entry size: %x (expecting 16,32,64,128 or 256)\n",
    "invalid bad block list size: %x blocks (expecting < 4 blocks)\n",
    "invalid number of free inodes: %x (expecting < %x)\n",
    "invalid number of free blocks: %x (expecting < %x)\n",
    "invalid number of fs blocks: %x (expecting %x)\n",
    "invalid computed number of fs blocks: %x (expecting %x)\n",
    "warning: the file system was not correctly shuted down\n",
    "inode %d: %s block number out of range: %d\n",
    "inode %d: %s block [%d/%d/%d/%d] allocated past EOF\n",
    "inode %d: duplicate %s block [%d/%d/%d/%d]\n",
    "inode %d: reserved block %d used as a %s block\n",
    "unused inode %d with mode %o\n",
    "unused inode %d with %d link(s)\n",
    "in use block %d is marked free in the bitmap\n",
    "unused block %d is marked used in the bitmap\n",
    "inode %d has invalid size: %d\n",
    "in use inode %d has 0 links\n",
    "inode %d has invalid mode: %o\n",
    "can't find entry \".\"\n",
    "can't find entry \"..\"\n",
    "entry \".\" points to inode %d instead of the current directory inode\n",
    "entry \"..\" points to inode %d instead of the parent directory inode\n",
    "[%8d] %s (invalid inode)\n",
    "entry %s is not a directory (mode=%o)\n",
    "inode %d has %d link(s) instead of %d\n",
    "the superblock was not locked when last updated\n",
    "inode %d: invalid 'di_blocks' field: %d (expecting %d)\n",
    "inode %d: [%d bytes (%d blocks)] use %d blocks\n",
    "invalid blocks bitmap\n",
    "cluster %d: invalid inodes bitmap\n",
};


char *block_type[] =
{
    "direct",
    "single indirect",
    "double indirect",
    "triple indirect",
};


/*
 *    A funny looking array, helping us to find out how many bits of '1' are
 * in a byte.  See the count_bitmap() function below.
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


void do_exit(int);


void usage(void)
{
    fprintf(stderr, "usage: %s [options] device\n", program);
    fprintf(stderr, " -v        verbose\n");
    fprintf(stderr, " -i        displays aditional information");
    fprintf(stderr, " about some inodes\n");
    fprintf(stderr, " -r        ask for confirmations before repairing\n");
    fprintf(stderr, " -a        repairs without confirmations\n");
    fprintf(stderr, " -h        this help message\n");
}


void tty_init(void)
{    
    tcgetattr(TTY_OUTPUT, &old_term);

    signal(SIGTERM, do_exit);
    signal(SIGQUIT, do_exit);
    signal(SIGINT,  do_exit);

    new_term = old_term;
    new_term.c_iflag &= ~(ICRNL | IGNCR | INLCR | IGNBRK | BRKINT);
    new_term.c_oflag |= OPOST;
    new_term.c_lflag |= ISIG | NOFLSH | ECHO;
    new_term.c_lflag &= ~ICANON;
    new_term.c_cc[VMIN]  = 1;
    new_term.c_cc[VTIME] = 0;
    tcsetattr(TTY_OUTPUT, TCSADRAIN, &new_term);
}


void tty_end(void)
{
    tcsetattr(TTY_OUTPUT, TCSADRAIN, &old_term);
}


void do_exit(int code)
{
    tty_end();
    exit(code);
}


int repair_confirmation(void)
{
    if (auto_repair)
	return 1;

    if (repair)
    {
	char c;

	printf("repair (y/n) ? ");
	fflush(stdout);
	read(0, &c, 1);
	printf("\n");
	return (c == 'y' || c == 'Y');
    }

    return 0;
}


int count_bitmap(unsigned char *bitmap, int bitmap_size)
{
    int i, total = 0, limit = bitmap_size / 4;

    for (i = 0; i < limit; bitmap += sizeof(unsigned), i++)
    {
	if (*(unsigned *)bitmap == 0)
	    continue;

	if (*(unsigned *)bitmap == 0xFFFFFFFF)
	    total += 32;
	else
	    total += bits[bitmap[0]] +
		     bits[bitmap[1]] +
		     bits[bitmap[2]] +
		     bits[bitmap[3]];
    }

    return total;
}


unsigned checksum(unsigned char *data, int count)
{
    int i;
    unsigned chksum = 0;

    for (i = 0; i < count; i++)
	chksum += *data++;

    return chksum;
}


int check_superblock(void)
{
    unsigned s;			/* superblock checksum.  */
    unsigned total_inodes;	/* total number of inodes in the fs.  */
    unsigned total_blocks;	/* total number of data blocks in the fs.  */
    unsigned total_fs_blocks;	/* total number of blocks in the fs.  */


    if (sb->s_magic != TFS_MAGIC_V02)
    {
	fprintf(stderr, MSG_INVALID_TFS_MAGIC, sb->s_magic, TFS_MAGIC_V02);
	return -1;
    }

    s = checksum((unsigned char *)sb,
		 sizeof(superblock) - sizeof(sb->s_checksum));

    if (s != sb->s_checksum)
    {
	fprintf(stderr, MSG_INVALID_CHECKSUM, sb->s_checksum, s);
	return -1;
    }

    if (sb->s_blksz != fs_blksz)
    {
	fprintf(stderr, MSG_INVALID_BLKSZ, sb->s_blksz);
	return -1;
    }

    if (sb->s_direntsz != 0x10 &&
	sb->s_direntsz != 0x20 &&
	sb->s_direntsz != 0x40 &&
	sb->s_direntsz != 0x80 &&
	sb->s_direntsz != 0x100)
    {
	fprintf(stderr, MSG_INVALID_DIRENTSZ, sb->s_blksz);
	return -1;
    }

    /* s_fsize should be checked using an ioctl() system call to get
       the device size.  */

    total_inodes = sb->s_clusters * sb->s_icluster;
    total_blocks = (sb->s_clusters - 1) * sb->s_bcluster +
		   (sb->s_lcflag ? sb->s_blcluster : sb->s_bcluster);

    if (sb->s_bblsz > (TFS_MAX_BAD_BLOCKS * sizeof(unsigned)) / sb->s_blksz)
    {
	fprintf(stderr, MSG_INVALID_BBLSZ, sb->s_bblsz);
	return -1;
    }

    if (sb->s_ifree > total_inodes)
    {
	fprintf(stderr, MSG_INVALID_IFREE, sb->s_ifree, total_inodes);
	sb_errors++;
    }

    if (sb->s_bfree > total_blocks)
    {
	fprintf(stderr, MSG_INVALID_BFREE, sb->s_bfree, total_blocks);
	sb_errors++;
    }

    total_fs_blocks = 1 + 1 + sb->s_ssbldsz + sb->s_bblsz +
		      (1+1+1+(sb->s_icluster*sizeof(dinode))/sb->s_blksz) *
		      sb->s_clusters +
		      sb->s_bcluster * (sb->s_clusters - 1) +
		      (sb->s_lcflag ? sb->s_blcluster : sb->s_bcluster) +
		      sb->s_ublocks;

    if (sb->s_fsize != fs_size)
    {
	fprintf(stderr, MSG_INVALID_FSIZE, sb->s_fsize, fs_size);
	return -1;
    }

    if (total_fs_blocks != fs_size)
    {
	fprintf(stderr, MSG_INVALID_CFSIZE, total_fs_blocks, fs_size);
	return -1;
    }

    if (!sb->s_flock || !sb->s_ilock)
    {
	fprintf(stderr, MSG_SUPERBLOCK_NOT_LOCKED);
	sb_errors++;
    }

    if(sb->s_fmod)
    {
	sb->s_flock = 0;
	sb->s_ilock = 0;
        sb->s_fmod  = 0;
	sb_errors++;
	fprintf(stderr, MSG_INVALID_SHUTDOWN);
    }

    if (verbose)
    {
	printf("File system type           : %s\n", sb->s_ftype);
	printf("File system name           : %s\n", sb->s_fname);
	printf("superblock checksum        : %x\n", sb->s_checksum);
	printf("file system magic number   : %x\n", sb->s_magic);
	printf("blocks in the entire volume: %d\n", sb->s_fsize);
	printf("bytes per file system block: %d\n", sb->s_blksz);
	printf("clusters                   : %d\n", sb->s_clusters);
	printf("inodes in a cluster        : %d\n", sb->s_icluster);
	printf("blocks in a cluster        : %d\n", sb->s_bcluster);
	printf("bad blocks list size       : %d\n", sb->s_bblsz);
	printf("free blocks                : %d\n", sb->s_bfree);
	printf("free inodes                : %d\n", sb->s_ifree);
	printf("bytes in a directory entry : %d\n", sb->s_direntsz);
    }

    return sb_errors;
}


int update_superblock(void)
{
    sb->s_checksum = checksum((unsigned char *)sb,
			      sizeof(superblock) - sizeof(sb->s_checksum));

    lseek(handle, fs_blksz, SEEK_SET);
    return write(handle, sb, sizeof(superblock)) == sizeof(superblock);
}


int mark_block(int block)
{
    int old_status;
    int home_cluster;

    home_cluster = (block - (1+1+sb->s_ssbldsz+sb->s_bblsz)) / sb->s_bpcluster;

    block = sb->s_bcluster * home_cluster + fs2cluster(block);

    old_status = bitmap[block / 32] & (1 << (block % 32));

    bitmap[block / 32] &= ~(1 << (block % 32));
    return old_status;
}


int is_free(unsigned *xbitmap, int index)
{
    return xbitmap[index / 32] & (1 << (index % 32));
}


int get_block_from_inode(dinode *di, int index)
{
    return (*(unsigned *)&di->di_addr[(index << 1) + index]) & 0xFFFFFF;
}


int reserved_block(int block)
{
    if (block < 1 + 1 + sb->s_ssbldsz + sb->s_bblsz)
	return 1;

    if (((block - (1 + 1 + sb->s_ssbldsz + sb->s_bblsz)) % sb->s_bpcluster) <
        (1 + 1 + 1 + ((sb->s_icluster * sizeof(dinode)) / sb->s_blksz)))
	return 1;

    return 0;
}


void insert_block(unsigned block, int type)
{
    if (block == 0)
	return;

    if (block < sb->s_fsize)
    {
	if (reserved_block(block))
	{
	    fprintf(stderr, MSG_RESERVED_BLOCK,
		    fs_inode, block, block_type[type]);
	    fs_errors++;
	    return;
	}

	if (type == 0)
	    inode_data_blocks++;

	inode_total_blocks++;

	if (size <= 0)
	{
	    if (type) block = 0;
	    fprintf(stderr, MSG_BLOCK_PAST_EOF, fs_inode, block_type[type],
		    ti_block, di_block, si_block, block);
	}
	else
	    if (mark_block(block) == 0)
	    {
		if (type) block = 0;
		fprintf(stderr, MSG_DUPLICATE_BLOCK,
			fs_inode, block_type[type],
			ti_block, di_block, si_block, block);
	    }
	    else
		return;
    }
    else
	fprintf(stderr, MSG_OUT_OF_RANGE, fs_inode, block_type[type], block);

    fs_errors++;
}


void single_indirect_block_insert(void)
{
    int i;

    if (si_block == 0)
	return;

    if (si_block < sb->s_fsize)
    {
	if (reserved_block(si_block))
	{
	    fprintf(stderr, MSG_RESERVED_BLOCK,
		    fs_inode, block_type[1], si_block);
	    fs_errors++;
	    return;
	}

	insert_block(si_block, 1);

	lseek(handle, si_block * sb->s_blksz, SEEK_SET);
	read(handle, si_blockp, sb->s_blksz);

	for (i = 0; i < sb->s_blksz/sizeof(unsigned); i++, size -= sb->s_blksz)
	    insert_block(si_blockp[i], 0);
    }
    else
    {
	fprintf(stderr, MSG_OUT_OF_RANGE, fs_inode, block_type[1], si_block);
	fs_errors++;
    }
}


void double_indirect_block_insert(void)
{
    int i;

    if (di_block == 0)
	return;

    if (di_block < sb->s_fsize)
    {
	if (reserved_block(di_block))
	{
	    fprintf(stderr, MSG_RESERVED_BLOCK,
		    fs_inode, block_type[2], di_block);
	    fs_errors++;
	    return;
	}

	si_block = 0;

	insert_block(di_block, 2);

	lseek(handle, di_block * sb->s_blksz, SEEK_SET);
	read(handle, di_blockp, sb->s_blksz);

	for (i = 0; i < sb->s_blksz / sizeof(unsigned); i++)
	{
	    si_block = di_blockp[i];
	    single_indirect_block_insert();
	}
    }
    else
    {
	fprintf(stderr, MSG_OUT_OF_RANGE, fs_inode, block_type[2], di_block);
	fs_errors++;
    }
}


void triple_indirect_block_insert(void)
{
    int i;

    if (ti_block == 0)
	return;

    if (ti_block < sb->s_fsize)
    {
	if (reserved_block(ti_block))
	{
	    fprintf(stderr, MSG_RESERVED_BLOCK,
		    fs_inode, block_type[3], ti_block);
	    fs_errors++;
	    return;
	}

	di_block = 0;

	insert_block(ti_block, 3);

	lseek(handle, ti_block * sb->s_blksz, SEEK_SET);
	read(handle, ti_blockp, sb->s_blksz);

	for (i = 0; i < sb->s_blksz / sizeof(unsigned); i++)
	{
	    di_block = ti_blockp[i];
	    double_indirect_block_insert();
	}
    }
    else
    {
	fprintf(stderr, MSG_OUT_OF_RANGE, fs_inode, block_type[3], ti_block);
	fs_errors++;
    }
}


int load_inode(int inode, dinode *di)
{
    int c = inode / sb->s_icluster;
    int i = inode % sb->s_icluster;

    lseek(handle,
	  cluster_reserved2fs(c, 1 + 1 + 1) * sb->s_blksz +
	  i * sizeof(dinode),
	  SEEK_SET);
    return (read(handle, di, sizeof(dinode)) == sizeof(dinode));
}


int save_inode(int inode, dinode *di)
{
    int c = inode / sb->s_icluster;
    int i = inode % sb->s_icluster;

    lseek(handle,
	  cluster_reserved2fs(c, 1 + 1 + 1) * sb->s_blksz +
	  i * sizeof(dinode),
	  SEEK_SET);
    return (write(handle, di, sizeof(dinode)) == sizeof(dinode));
}


void check_cluster_inodes(void)
{
    int i;
    int rounded_size;
    int free_inodes;	/* cluster ibitmap free inodes.  */
    int ibitmap_block;	/* fs block containing the cluster inode bitmap.  */
    int saved_fs_errors;


    ibitmap_block = cluster_reserved2fs(cluster, TFS_INODES_BITMAP);
    lseek(handle, ibitmap_block * sb->s_blksz, SEEK_SET);
    read(handle, ibitmap, sb->s_icluster / 8);

    lseek(handle, cluster_reserved2fs(cluster, 1+1+1) * sb->s_blksz, SEEK_SET);
    read(handle, cluster_inodes, sb->s_icluster * sizeof(dinode));

    free_inodes = count_bitmap((unsigned char *)ibitmap, sb->s_icluster / 8);
    total_free_inodes += free_inodes;

    if (verbose)
	printf("cluster %d: %d free inodes\n", cluster, free_inodes);

    for (inode = cluster ? 0 : 1; inode < sb->s_icluster; inode++)
    {
	if (is_free(ibitmap, inode))
	{
	    if (cluster_inodes[inode].di_mode)
	    {
		fs_errors++;
		fprintf(stderr, MSG_FREE_INODE_HAS_MODE,
			cluster * sb->s_icluster + inode,
			cluster_inodes[inode].di_mode);

		if (!repair_confirmation())
		    continue;

		fs_errors--;
		cluster_inodes[inode].di_mode = 0;
		save_inode(fs_inode, &cluster_inodes[inode]);
		printf("fixed\n");
	    }

	    /* We will not count this as an error because we will
	       try to fix it at stage 4.  */

	    if (cluster_inodes[inode].di_nlinks)
		fprintf(stderr, MSG_FREE_INODE_HAS_NLINKS,
			cluster * sb->s_icluster + inode,
			cluster_inodes[inode].di_nlinks);
	    continue;
	}
	else
	{
	    /* We will not count this as an error because we will
	       try to fix it at stage 4.  */

	    if (cluster_inodes[inode].di_nlinks == 0)
		fprintf(stderr, MSG_INVALID_INODE_NLINKS_0,
			cluster * sb->s_icluster + inode);

	    if (!S_ISSOCK(cluster_inodes[inode].di_mode) &&
		!S_ISLNK(cluster_inodes[inode].di_mode)  &&
		!S_ISREG(cluster_inodes[inode].di_mode)  &&
		!S_ISBLK(cluster_inodes[inode].di_mode)  &&
		!S_ISDIR(cluster_inodes[inode].di_mode)  &&
		!S_ISCHR(cluster_inodes[inode].di_mode)  &&
		!S_ISFIFO(cluster_inodes[inode].di_mode))
	    {
		fs_errors++;
		fprintf(stderr, MSG_INVALID_INODE_MODE,
			cluster * sb->s_icluster + inode,
			cluster_inodes[inode].di_mode);

		if (!repair_confirmation())
		    continue;

		fs_errors--;
		cluster_inodes[inode].di_mode = S_IFREG;
		save_inode(fs_inode, &cluster_inodes[inode]);
		printf("fixed\n");
	    }
	}

	inode_total_blocks = inode_data_blocks = 0;

	size = cluster_inodes[inode].di_size;

	if (size % sb->s_blksz)
	    size += sb->s_blksz - (size % sb->s_blksz);

	rounded_size = size;

	si_block = di_block = ti_block = 0;

	saved_fs_errors = fs_errors;

	for (i = 0; i < DIRECT_BLOCKS; i++, size -= sb->s_blksz)
	    insert_block(get_block_from_inode(&cluster_inodes[inode], i), 0);

	si_block = get_block_from_inode(&cluster_inodes[inode],
					SINGLE_INDIRECT_BLOCK);
	single_indirect_block_insert();

	di_block = get_block_from_inode(&cluster_inodes[inode],
					DOUBLE_INDIRECT_BLOCK);
	double_indirect_block_insert();

	ti_block = get_block_from_inode(&cluster_inodes[inode],
					TRIPLE_INDIRECT_BLOCK);
	triple_indirect_block_insert();

	if (fs_errors > saved_fs_errors && repair_confirmation())
	{
	    /* A very severe error has been deteted, we should get rid of
	       the current inode.  */
	    memset(&cluster_inodes[inode], 0, sizeof(dinode));
	    save_inode(fs_inode, &cluster_inodes[inode]);
	    fprintf(stderr, "don't know how to fix inode %d\n", fs_inode);
	}

	if (inode_total_blocks != cluster_inodes[inode].di_blocks)
	{
	    fs_errors++;
	    fprintf(stderr, MSG_INVALID_DI_BLOCKS, inode,
		    cluster_inodes[inode].di_blocks, inode_total_blocks);

	    if (!repair_confirmation())
		continue;

	    fs_errors--;
	    cluster_inodes[inode].di_blocks = inode_total_blocks;
	    save_inode(fs_inode, &cluster_inodes[inode]);
	    printf("fixed\n");
	}

	if (inodes_info && (rounded_size != inode_data_blocks * sb->s_blksz))
	    fprintf(stderr, MSG_INODE_USE_BLOCKS, inode,
		    cluster_inodes[inode].di_size,
		    rounded_size / sb->s_blksz, inode_data_blocks);
    }
}


void check_cluster_blocks(void)
{
    int i, j;
    int modified;
    int bitmap_size;
    int bfree_blocks;
    unsigned *cbitmap;
    int bbitmap_status;
    int cbitmap_status;


    bitmap_size = cluster_data_blocks(cluster) / 8;

    lseek(handle,
	  (1 + 1 + sb->s_ssbldsz + sb->s_bblsz +
	   cluster * sb->s_bpcluster + 1 + 1) * sb->s_blksz,
	  SEEK_SET);
    read(handle, bbitmap, bitmap_size);

    cbitmap = bitmap + (cluster * sb->s_bcluster / 8) / sizeof(unsigned);

    if (verbose)
	printf("cluster %d: ", cluster);

    if (memcmp(cbitmap, bbitmap, bitmap_size) == 0)
    {
	bfree_blocks = count_bitmap((unsigned char *)bbitmap, bitmap_size);
	total_free_blocks += bfree_blocks;

	if (verbose)
	    printf("%d free blocks\n", bfree_blocks);

	return;
    }

    fs_errors++;
    fprintf(stderr, MSG_INVALID_BLOCKS_BITMAP);

    modified = 0;

    for (i = 0; i < bitmap_size / sizeof(unsigned); i++)
    {
	if (bbitmap[i] == cbitmap[i])
	    continue;

	for (j = 0; j < 32; j++)
	{
	    bbitmap_status = bbitmap[i] & (1 << j);
	    cbitmap_status = cbitmap[i] & (1 << j);

	    if (bbitmap_status == cbitmap_status)
		continue;

	    fprintf(stderr, bbitmap_status ? MSG_BLOCK_MARKED_FREE :
					     MSG_BLOCK_MARKED_USED,
			    cluster2fs(cluster, i * 32 + j));

	    if (!repair_confirmation())
		continue;

	    if (cbitmap_status)
	    {
		bbitmap[i] |=  (1 << j);
		sb->s_bfree++;
	    }
	    else
	    {
		bbitmap[i] &= ~(1 << j);
		sb->s_bfree--;
	    }

	    printf("fixed\n");

	    modified = 1;
	}
    }

    if (modified)
    {
	lseek(handle,
	      (1 + 1 + sb->s_ssbldsz + sb->s_bblsz +
	       cluster * sb->s_bpcluster + 1 + 1) * sb->s_blksz,
	      SEEK_SET);
	write(handle, bbitmap, bitmap_size);
	update_superblock();

	if (memcmp(cbitmap, bbitmap, bitmap_size) == 0)
	    fs_errors--;
    }

    bfree_blocks = count_bitmap((unsigned char *)bbitmap, bitmap_size);
    total_free_blocks += bfree_blocks;

    if (verbose)
	printf("%d free blocks\n", bfree_blocks);
}


int directory_read(dinode *di, char *buf, int offset, int count)
{
    int block;

    if (di->di_size >= sb->s_blksz * DIRECT_BLOCKS)
	fprintf(stderr, "directory too big (for me)\n");

    block = get_block_from_inode(di, offset / sb->s_blksz);

    lseek(handle, block * sb->s_blksz + offset % sb->s_blksz, SEEK_SET);
    return (read(handle, buf, sb->s_direntsz) == sb->s_direntsz);
}


void parse_tfs_tree(int inode, int parent_inode, dinode *di, char *path)
{
    dinode _di;
    char *_path;
    tfs_dirent *d;
    int i, i_size, len;


    i_size = di->di_size;

    if ((i_size < 2 * sb->s_direntsz) || (i_size % sb->s_direntsz))
	fprintf(stderr, MSG_INVALID_INODE_SIZE, inode, i_size);

    d = (tfs_dirent *)malloc(sb->s_direntsz + 1);
    len = strlen(path);
    _path = (char *)malloc(len + sb->s_direntsz + 1);
    strcpy(_path, path);
    _path[len++] = '/';
    _path[len] = 0;

    directory_read(di, (char *)d, 0, sb->s_direntsz);

    if (d->name[0] != '.' || d->name[1] != 0)
	fprintf(stderr, MSG_NO_DOT_ENTRY);
    else
    {
	if (d->inode != inode)
	    fprintf(stderr, MSG_INVALID_DOT_ENTRY, d->inode);

	d->name[sb->s_direntsz - sizeof(__ino_t)] = 0;

	if (d->inode > sb->s_clusters * sb->s_icluster)
	{
	    strcpy(&_path[len], d->name);
	    fprintf(stderr, MSG_INVALID_INODE_NUMBER, d->inode, _path);
	}
	else
	{
	    load_inode(d->inode, &_di);

	    i_nlinks[d->inode]++;

	    if (!S_ISDIR(_di.di_mode))
		fprintf(stderr, MSG_ENTRY_NOT_DIR, d->name, _di.di_mode);
	}
    }

    directory_read(di, (char *)d, sb->s_direntsz, sb->s_direntsz);

    if (d->name[0] != '.' || d->name[1] != '.' || d->name[2] != 0)
	fprintf(stderr, MSG_NO_DOTDOT_ENTRY);
    else
    {
	if (d->inode != parent_inode)
	    fprintf(stderr, MSG_INVALID_DOTDOT_ENTRY, d->inode);

	d->name[sb->s_direntsz - sizeof(__ino_t)] = 0;

	if (d->inode > sb->s_clusters * sb->s_icluster)
	{
	    strcpy(&_path[len], d->name);
	    fprintf(stderr, MSG_INVALID_INODE_NUMBER, d->inode, _path);
	}
	else
	{
	    load_inode(d->inode, &_di);

	    i_nlinks[d->inode]++;

	    if (!S_ISDIR(_di.di_mode))
		fprintf(stderr, MSG_ENTRY_NOT_DIR, d->name, _di.di_mode);
	}
    }

    for (i = 2 * sb->s_direntsz; i < i_size; i += sb->s_direntsz)
    {
	directory_read(di, (char *)d, i, sb->s_direntsz);

	if (d->inode == 0)
	    continue;

	d->name[sb->s_direntsz - sizeof(__ino_t)] = 0;

	if (d->inode > sb->s_clusters * sb->s_icluster)
	{
	    strcpy(&_path[len], d->name);
	    fprintf(stderr, MSG_INVALID_INODE_NUMBER, d->inode, _path);
	    continue;
	}

	load_inode(d->inode, &_di);

	i_nlinks[d->inode]++;

	if (_di.di_nlinks == 0)
	    fprintf(stderr, MSG_INVALID_INODE_NLINKS_0, d->inode);

	if (S_ISDIR(_di.di_mode))
	{
	    if (d->name[0] == '.' && d->name[1] == 0)
		continue;

	    if (d->name[0] == '.' && d->name[1] == '.' && d->name[2] == 0)
		continue;

	    strcpy(&_path[len], d->name);

	    if (verbose)
		printf("[%8d %6o %10d] %s\n",
		       d->inode, _di.di_mode, _di.di_size, _path);

	    parse_tfs_tree(d->inode, inode, &_di, _path);
	}
	else
	{
	    strcpy(&_path[len], d->name);

	    if (verbose)
		printf("[%8d %6o %10d] %s\n",
		       d->inode, _di.di_mode, _di.di_size, _path);
	}
    }

    free(d);
    free(_path);
}


void check_nlinks(void)
{
    int i;
    dinode di;
    int modified = 0;
    int ibitmap_block;


    ibitmap_block = cluster_reserved2fs(cluster, TFS_INODES_BITMAP);
    lseek(handle, ibitmap_block * sb->s_blksz, SEEK_SET);
    read(handle, ibitmap, sb->s_icluster / 8);

    for (inode = cluster ? 0 : 1; inode < sb->s_icluster; inode++)
    {
	load_inode(i = fs_inode, &di);

	if (di.di_nlinks != i_nlinks[i])
	{
	    fs_errors++;
	    fprintf(stderr, MSG_INVALID_INODE_NLINKS,
		    i, di.di_nlinks, i_nlinks[i]);

	    if (!repair_confirmation())
		continue;

	    fs_errors--;
	    di.di_nlinks = i_nlinks[i];
	    save_inode(i, &di);
	    printf("fixed\n");
	}

	if (i_nlinks[i])
	{
	    if (is_free(ibitmap, inode))
		modified = 1;
	    ibitmap[inode / 32] &= ~(1 << (inode % 32));
	}
	else
	{
	    if (!is_free(ibitmap, inode))
		modified = 1;
	    ibitmap[inode / 32] |= (1 << (inode % 32));
	}
    }

    if (modified)
    {
	fs_errors++;
	fprintf(stderr, MSG_INVALID_INODES_BITMAP, cluster);

	if (!repair_confirmation())
	    return;

	fs_errors--;
	lseek(handle, ibitmap_block * sb->s_blksz, SEEK_SET);
	write(handle, ibitmap, sb->s_icluster / 8);
	printf("fixed\n");
    }
}


int main(int argc, char *argv[])
{
    char sz;
    int r, c;
    dinode root_di;
    int data_blocks;
    struct stat statbuf;
    char *device_name = NULL;


    program = argv[0];

    printf("tfsck %d.%d, "__TIME__" "__DATE__"\n",
	   TFS_VERSION, TFS_PATCHLEVEL);

    if (getuid())
    {
        fprintf(stderr, "%s: only the super user can do this\n", program);
        return ERR_NOT_SUPERUSER;
    }

    while ((c = getopt(argc, argv, "virah")) != -1)
        switch(c)
        {
	    case 'v':

	        /* Verbose.  */
		verbose = 1;
		break;

	    case 'i':

		/* Auxiliary inode info.  */
		inodes_info = 1;
		break;

	    case 'r':

		/* Ask confirmation before repairing.  */
	        repair = 1;
	        break;

	    case 'a':

		/* Repairs without confirmations.  */
	        auto_repair = 1;
	        break;

	    case 'h':

		/* Help request.  */
		usage();
		return ERR_USAGE_REQUEST;

	    case '?':

		if (isprint(optopt))
		    fprintf(stderr, "%s: unknown option `-%c'\n",
		    	    argv[0], optopt);
		else
		    fprintf(stderr, "%s: unknown character `\\x%x'\n",
		    	    argv[0], optopt);
		return ERR_UNKNOWN_OPTION;

	    default:

		fprintf(stderr, "%s unknown error\n", argv[0]);
	    	return ERR_UNKNOWN_ERROR;
        }

    if (repair && auto_repair)
    {
	fprintf(stderr, "%s: you can't give both -r and -a\n", program);
	return ERR_R_A;
    }

    if (repair && !isatty(0))
    {
	fprintf(stderr, "%s: need tty to run interactively\n", program);
	return ERR_NO_TTY;
    }

    if (optind < argc)
    {
	device_name = malloc(strlen(argv[optind]) + 1);
	strcpy(device_name, argv[optind]);
    }
    else
    {
	fprintf(stderr, "%s: no device specified\n", program);
	return ERR_NO_DEVICE;
    }

    /* We should get the device size with an ioctl() system call here.  */ 
    if (strcmp(device_name, "/dev/fd0") == 0)
	fs_size = 1440 * 1024;
    else
	if (strcmp(device_name, "/dev/fd1") == 0)
	    fs_size = 1200 * 1024;
	else
	    if (strcmp(device_name, THIX_PARTITION_NAME) == 0)
		fs_size = THIX_PARTITION_SIZE * 1024;
	    else
	    {
		fprintf(stderr, "%s: unknown device %s\n",
			program, device_name);
		return ERR_UNKNOWN_DEVICE;
	    }

    handle = open(device_name, O_RDWR);

    if (handle == -1)
    {
	fprintf(stderr, "%s: can't open the specified device (%s)\n",
		program, device_name);
	return ERR_OPEN_DEVICE;
    }

    r = read(handle, sb, 1);

    if (r == 0 || r == -1)
    {
	fprintf(stderr, "%s: I/O error on %s\n", program , device_name);
	return ERR_IO_ERROR;
    }

    fstat(handle, &statbuf);

    if (!S_ISBLK(statbuf.st_mode))
    {
	fprintf(stderr, "%s: block device required\n", program);
	return ERR_NEED_BLOCK_DEVICE;
    }

    lseek(handle, 0x1fd, SEEK_SET);
    read(handle, &sz, 1);
    fs_blksz = (int)sz * 0x200;

    if (fs_blksz != 0x200 &&
	fs_blksz != 0x400 &&
	fs_blksz != 0x800)
    {
	fprintf(stderr, "%s: invalid file system size in boot block\n",
		program);
	return ERR_INVALID_BBFS_BLKSZ;
    }

    fs_size /= fs_blksz;

    tty_init();

    if (verbose)
	printf("[stage 1 - checking the superblock]\n");

    lseek(handle, fs_blksz, SEEK_SET);
    read(handle, sb, sizeof(superblock));

    if (check_superblock() < 0)
	do_exit(ERR_INVALID_SUPERBLOCK);

    data_blocks = (sb->s_clusters - 1) * sb->s_bcluster +
    		  (sb->s_lcflag ? sb->s_blcluster : sb->s_bcluster);

    si_blockp	   = (unsigned *)calloc(sb->s_blksz, 1);
    di_blockp	   = (unsigned *)calloc(sb->s_blksz, 1);
    ti_blockp	   = (unsigned *)calloc(sb->s_blksz, 1);
    ibitmap	   = (unsigned *)calloc(sb->s_blksz, 1);
    bbitmap	   = (unsigned *)calloc(sb->s_blksz, 1);
    cluster_inodes = (dinode   *)calloc(sb->s_icluster * sizeof(dinode), 1);
    bitmap	   = (unsigned *)malloc(data_blocks / 8);

    if (ibitmap        == NULL ||
	bbitmap        == NULL ||
	cluster_inodes == NULL ||
	si_blockp      == NULL ||
	di_blockp      == NULL ||
	ti_blockp      == NULL ||
	bitmap	       == NULL)
    {
	fprintf(stderr, "%s: not enough memory\n", program);
	do_exit(ERR_NOT_ENOUGH_MEMORY);
    }

    memset(bitmap, 0xFF, data_blocks / 8);

    if (verbose)
	printf("[stage 2 - checking inodes and blocks]\n");

    for (cluster = 0; cluster < sb->s_clusters; cluster++)
	check_cluster_inodes();

    for (cluster = 0; cluster < sb->s_clusters; cluster++)
	check_cluster_blocks();

    if (total_free_blocks != sb->s_bfree)
    {
	fs_errors++;
	fprintf(stderr, "invalid free blocks count in superblock: "
		        "s_bfree=%d, counted=%d\n",
		sb->s_bfree, total_free_blocks);

	if (repair_confirmation())
	{
	    sb->s_bfree = total_free_blocks;
	    update_superblock();
	    fs_errors--;
	}
    }

    if (total_free_inodes != sb->s_ifree)
    {
	fs_errors++;
	fprintf(stderr, "invalid free inodes count in superblock: "
		        "s_ifree=%d, counted=%d\n",
		sb->s_ifree, total_free_inodes);

	if (repair_confirmation())
	{
	    sb->s_ifree = total_free_inodes;
	    update_superblock();
	    fs_errors--;
	}
    }

    free(bitmap);
    free(cluster_inodes);
    free(bbitmap);

    if (fs_errors)
    {
	fprintf(stderr, "%s: errors detected in stage 2, "
		        "can't perform stage 3\n", program);
	fprintf(stderr, "%s: if all the stage 2 errors have been corrected, "
		        "run %s again\n", program, program);
	do_exit(ERR_STAGE1_ERRORS);
    }

    if (verbose)
	printf("[stage 3 - checking files and directories]\n");

    i_nlinks = (unsigned short *)calloc(sb->s_clusters * sb->s_icluster,
					sizeof(unsigned short));

    if (i_nlinks == NULL)
    {
	fprintf(stderr, "%s: not enough memory\n", program);
	do_exit(ERR_NOT_ENOUGH_MEMORY);
    }

    load_inode(1, &root_di);
    parse_tfs_tree(1, 1, &root_di, "");

    if (verbose)
	printf("[stage 4 - rechecking inodes for invalid number of links]\n");

    for (cluster = 0; cluster < sb->s_clusters; cluster++)
	check_nlinks();

    sb->s_state = TFS_MAGIC_V02;
    update_superblock();

    return 0;
}
