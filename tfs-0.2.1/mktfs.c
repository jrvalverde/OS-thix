
/*
 *
 *	Thix Operating System
 *	Copyright (c) 1993,1994 Tudor Hulubei
 *
 *	mktfs.c
 *	Make Thix File System (TFS) utility.
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


/* Just temporary.  */
#include "../thix/include/thix/fs.h"
#include "../thix/include/thix/inode.h"


/* Just temporary.  */
#define THIX_PARTITION_NAME "/dev/hda3"


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
#define ERR_NO_BLOCKS		  15


#define DEFAULT_BLKSZ		1024
#define DEFAULT_MAXBADBLOCKS	   0
#define DEFAULT_ICLUSTER	1024
#define DEFAULT_SSBLDSZ		   0
#define DEFAULT_DIRENTSZ	  32
#define DEFAULT_FNAME	  "Software"


char *program;
superblock sb;
int simulate = 0;
int superblock_always = 0;
unsigned char buf[16384];


void usage(void)
{
    fprintf(stderr, "usage: %s [options] device blocks\n\n", program);
    fprintf(stderr, "'device' is the device name (e.g. /dev/hda4)\n");
    fprintf(stderr, "'blocks' is the number of 1K blocks\n\n");
    fprintf(stderr, " -b xxx    block size                ");
    fprintf(stderr, "(default = %d)\n", DEFAULT_BLKSZ);
    fprintf(stderr, " -B xxx    maximum bad blocks        ");
    fprintf(stderr, "(default = %d)\n", DEFAULT_MAXBADBLOCKS);
    fprintf(stderr, " -d xxx    directory entry size      ");
    fprintf(stderr, "(default = %d)\n", DEFAULT_DIRENTSZ);
    fprintf(stderr, " -i xxx    inodes per cluster        ");
    fprintf(stderr, "(default = %d)\n", DEFAULT_ICLUSTER);
    fprintf(stderr, " -k xxx    kernel size (in 1K blocks)");
    fprintf(stderr, "(default = %d)\n", DEFAULT_SSBLDSZ);
    fprintf(stderr, " -n xxx    file system name          ");
    fprintf(stderr, "(default = \"%s\")\n", DEFAULT_FNAME);
    fprintf(stderr, " -s        simulate only\n");
    fprintf(stderr, " -S        always write the superblock\n");
    fprintf(stderr, " -h        this help message\n");
}


int __write__(int handle, void *buffer, int count)
{
    if (simulate)
	return count;

    return write(handle, buffer, count);
}


int main(int argc, char *argv[])
{
    dinode *di;
    int handle, i, j;
    int cluster, r, c;
    int max_bad_blocks;
    unsigned chksum = 0;
    struct stat statbuf;
    int fs_blocks;
    int first_data_block;
    char *device_name = NULL;
    int cluster_reserved_blocks;
    int blocks_used_for_clusters;


    program = argv[0];

    fprintf(stderr, "mktfs %d.%d, "__TIME__" "__DATE__"\n",
	   TFS_VERSION, TFS_PATCHLEVEL);

    /* clear the superblock structure. */
    memset(&sb, 0, sizeof(superblock));

    /* setup some defaults. */
    sb.s_state	     = TFS_MAGIC_V02;
    sb.s_blksz	     = DEFAULT_BLKSZ;
    sb.s_icluster    = DEFAULT_ICLUSTER;
    sb.s_ssbldsz     = DEFAULT_SSBLDSZ;
    sb.s_direntsz    = DEFAULT_DIRENTSZ;
    max_bad_blocks   = DEFAULT_MAXBADBLOCKS;
    strcpy(sb.s_fname, DEFAULT_FNAME);

    if (getuid())
    {
        printf("%s: only the super user can do this\n", program);
        return ERR_NOT_SUPERUSER;
    }

    while ((c = getopt(argc, argv, "b:B:d:i:k:n:sSh")) != -1)
        switch(c)
        {
	    case 'b':		/* block size.  */

		sb.s_blksz = atoi(optarg);
	    	break;

	    case 'B':		/* block size.  */

		max_bad_blocks = atoi(optarg);
	    	break;

	    case 'd':		/* directory entry size.  */

		sb.s_direntsz = atoi(optarg);
		break;

	    case 'i':		/* inodes in a cluster.  */

		sb.s_icluster = atoi(optarg);
		break;

	    case 'k':		/* kernel size.  */
		sb.s_ssbldsz = atoi(optarg);
		break;

	    case 'n':		/* file system name.  */

		strncpy(sb.s_fname, optarg, sizeof(sb.s_fname) - 1);
		break;

	    case 's':
		simulate = 1;	/* only simulate.  */
		break;

	    case 'S':
		superblock_always = 1;	/* always write the superblock.  */
		break;

	    case 'h':

		usage();	/* help request.  */
		return ERR_USAGE_REQUEST;

	    case '?':

		if (isprint(optopt))
		    fprintf(stderr, "%s: unknown option `-%c'\n",
		    	    program, optopt);
		else
		    fprintf(stderr, "%s: unknown character `\\x%x'\n",
		    	    program, optopt);
		return ERR_UNKNOWN_OPTION;

	    default:

		fprintf(stderr, "%s unknown error\n", program);
	    	return ERR_UNKNOWN_ERROR;
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

    optind++;

    if (optind < argc)
	fs_blocks = atoi(argv[optind]);
    else
    {
        fprintf(stderr, "%s: no number of 1K blocks specified\n", program);
	return ERR_NO_BLOCKS;
    }

/*
    if (sb.s_blksz != 0x200 &&
	sb.s_blksz != 0x400 &&
	sb.s_blksz != 0x800)
    {
	fprintf(stderr, "%s: invalid block size: %d\n", program, sb.s_blksz);
	return ERR_BAD_BLOCK_SIZE;
    }
*/

    if (sb.s_blksz != 0x400)
    {
	fprintf(stderr, "%s: invalid block size: %d\n", program, sb.s_blksz);
	fprintf(stderr, "%s: use 1024 K blocks\n", program);
	return ERR_BAD_BLOCK_SIZE;
    }
    
    /*
     *  We should get the device size here using an ioctl() system call. 
     */

    if (strcmp(device_name, "/dev/fd0") == 0)
	sb.s_fsize = (1440 * 1024) / sb.s_blksz;
    else
	if (strcmp(device_name, "/dev/fd1") == 0)
	    sb.s_fsize = (1200 * 1024) / sb.s_blksz;
	else
	    /* Just temporary.  */
	    if (strcmp(device_name, THIX_PARTITION_NAME) == 0)
		sb.s_fsize = (fs_blocks * 1024) / sb.s_blksz;
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

    r = read(handle, buf, 1);

    if (r == 0 || r == -1)
    {
	fprintf(stderr, "%s: I/O error on %s\n", program, device_name);
	return ERR_IO_ERROR;
    }

    fstat(handle, &statbuf);

    if (!S_ISBLK(statbuf.st_mode))
    {
	fprintf(stderr, "%s: block device required\n", program);
	return ERR_NEED_BLOCK_DEVICE;
    }

    sb.s_bcluster = sb.s_blksz * 8;

    if (sb.s_icluster > sb.s_blksz * 8)
    {
	fprintf(stderr, "%s: too many inodes/cluster: %d\n",
		program, sb.s_icluster);
	return ERR_TOO_MANY_INODES;
    }

    if (sb.s_icluster < sb.s_blksz / 16)
    {
	fprintf(stderr, "%s: warning: too few inodes/cluster: %d\n",
		program, sb.s_icluster);

	if (sb.s_icluster == 0)
	    return ERR_TOO_FEW_INODES;
    }

    if (max_bad_blocks < 0 || max_bad_blocks > TFS_MAX_BAD_BLOCKS)
    {
	fprintf(stderr, "%s: wrong max bad blocks number: %d\n",
		program, max_bad_blocks);
	return ERR_BAD_BAD_BLOCKS_NO;
    }

    sb.s_bblsz = (max_bad_blocks * sizeof(unsigned)) / sb.s_blksz +
    		 (((max_bad_blocks * sizeof(unsigned)) % sb.s_blksz) ? 1 : 0);

    if (sb.s_direntsz != 0x10 &&
	sb.s_direntsz != 0x20 &&
	sb.s_direntsz != 0x40 &&
	sb.s_direntsz != 0x80 &&
	sb.s_direntsz != 0x100)
    {
	fprintf(stderr, "%s: invalid directory entry size: %d\n",
		program, sb.s_direntsz);
	return ERR_DIR_ENTRY_SIZE;
    }

    if (sb.s_icluster % 32)
	sb.s_icluster += 32 - (sb.s_icluster % 32);

    cluster_reserved_blocks = 1 + 1 + 1 +
    			      (sb.s_icluster * sizeof(dinode)) / sb.s_blksz;

    sb.s_bpcluster = cluster_reserved_blocks + sb.s_bcluster;

    blocks_used_for_clusters = sb.s_fsize - 1 - 1 - sb.s_bblsz - sb.s_ssbldsz;

    sb.s_clusters = blocks_used_for_clusters / sb.s_bpcluster;

    if (blocks_used_for_clusters % sb.s_bpcluster >=
	cluster_reserved_blocks)
    {
	sb.s_clusters++;
        sb.s_lcflag    = 1;
	sb.s_blcluster = ((blocks_used_for_clusters % sb.s_bpcluster -
			 cluster_reserved_blocks) / 32) * 32;
    }
    else
    {
        sb.s_blcluster = 0;
        sb.s_lcflag    = 0;
    }

    sb.s_ifree = sb.s_clusters * sb.s_icluster;
    sb.s_bfree = (sb.s_clusters - 1) * sb.s_bcluster +
    		 (sb.s_lcflag ? sb.s_blcluster : sb.s_bcluster);

    sb.s_ifree -= 2;	/* inode 0 is not used, inode 1 is root dir. */
    sb.s_bfree--;	/* the first block is used for the root dir. */

    sb.s_time  = time(NULL);
    sb.s_state = TFS_OK;
    sb.s_magic = TFS_MAGIC_V02;
    sb.s_flock = 1;
    sb.s_ilock = 1;
    sprintf(sb.s_ftype, "TFS %d.%d", TFS_VERSION, TFS_PATCHLEVEL);

    sb.s_ublocks = sb.s_fsize -
		   (1 + 1 + sb.s_ssbldsz + sb.s_bblsz +
		    (1 + 1 + 1 + (sb.s_icluster*sizeof(dinode)) / sb.s_blksz) *
		    sb.s_clusters +
		    sb.s_bcluster * (sb.s_clusters - 1) +
		    (sb.s_lcflag ? sb.s_blcluster : sb.s_bcluster));

    for (i = 0; i < sizeof(superblock) - sizeof(sb.s_checksum); i++)
	chksum += ((unsigned char *)&sb)[i];

    sb.s_checksum = chksum;

    printf("\n%s [%s] on %s info:\n", sb.s_ftype, sb.s_fname, device_name);

    printf("block size     : %d\n", sb.s_blksz);
    printf("total blocks   : %d\n", sb.s_fsize);
    printf("data blocks    : %d\n", sb.s_bfree + 1);
    printf("inodes         : %d\n", sb.s_ifree + 2);
    printf("clusters       : %d\n", sb.s_clusters);
    printf("max bad blocks : %d\n", max_bad_blocks);

    printf("\n");

    printf("1 block reserved for the boot block\n");
    printf("%d block(s) reserved for the second stage boot loader\n",
	   sb.s_ssbldsz);
    printf("1 block reserved for the superblock [%x]\n", sb.s_checksum);
    printf("%d block(s) reserved for the bad blocks list\n", sb.s_bblsz);
    printf("%d blocks used for clusters\n",
	   blocks_used_for_clusters - sb.s_ublocks);

    printf("\nClusters info:\n");

    printf("superblock backup in the first block\n");
    printf("data blocks/cluster : %d\n", sb.s_bcluster);
    printf("inodes/cluster      : %d\n", sb.s_icluster);

    if (sb.s_lcflag)
	printf("last cluster blocks : %d\n", sb.s_blcluster);

    printf("\nWriting the boot block ... (not implemented yet)\n");
    buf[0x1fd] = sb.s_blksz / 512;
    lseek(handle, 0, SEEK_SET);
    __write__(handle, buf, sb.s_blksz);

    printf("Writing the superblock ...\n");
    lseek(handle, 1 * sb.s_blksz, SEEK_SET);

    if (superblock_always)
        write(handle, &sb, sizeof(superblock));
    else
	__write__(handle, &sb, sizeof(superblock));

    memset(buf, 0, sb.s_blksz - sizeof(superblock));
    __write__(handle, buf, sb.s_blksz - sizeof(superblock));

    printf("Writing the bad blocks list ... (not implemented yet)\n");
    lseek(handle, (1 + 1) * sb.s_blksz, SEEK_SET);

    /* In fact reads the bad blocks list from a file...  */
    memset(buf, 0, sb.s_bblsz * sb.s_blksz);
    __write__(handle, buf, sb.s_bblsz * sb.s_blksz);

    for (cluster = 0; cluster < sb.s_clusters; cluster++)
    {
	printf("Writing cluster %d ...\r", cluster);

	memcpy(buf, &sb, sizeof(superblock));
	memset(buf + sizeof(superblock), 0, sb.s_blksz - sizeof(superblock));
	memset(buf + sb.s_blksz, 0xFF, (1 + 1) * sb.s_blksz);

	if (cluster == 0)
	{
	    buf[sb.s_blksz]     = 0xfc;	/* inode 0 & 1 are reserved.  */
	    buf[sb.s_blksz * 2] = 0xfe;	/* first block = root dir.  */
	}

	lseek(handle, (1 + 1 + sb.s_ssbldsz + sb.s_bblsz) * sb.s_blksz +
		      sb.s_bpcluster * sb.s_blksz * cluster, SEEK_SET);
	__write__(handle, buf, (1 + 1 + 1) * sb.s_blksz);

	memset(buf, 0, sb.s_blksz);

	for (j = 0; j < (sb.s_icluster * sizeof(dinode)) / sb.s_blksz; j++)
	    __write__(handle, buf, sb.s_blksz);
    }

    printf("\nWriting the root directory inode ...\n");
    first_data_block = 1 + 1 + sb.s_ssbldsz + sb.s_bblsz +
		       1 + 1 + 1 + (sb.s_icluster*sizeof(dinode)) / sb.s_blksz;
    memset(buf, 0, sb.s_blksz);
    di = (dinode *)buf + 1;
    di->di_nlinks = 2;			/* "." & ".." point both to "/"  */
    di->di_blocks = 1;
    di->di_mode = S_IFDIR | 0755;
    di->di_size = sb.s_direntsz * 2;	/* space for two entries: "." & ".." */
    *(unsigned *)di->di_addr = first_data_block;
    di->di_atime = time(NULL);
    di->di_mtime = time(NULL);
    di->di_ctime = time(NULL);
    lseek(handle, (1 + 1 + sb.s_ssbldsz + sb.s_bblsz + 1 + 1 + 1) * sb.s_blksz,
	  SEEK_SET);
    __write__(handle, buf, sb.s_blksz);

    printf("Writing the root directory ...\n");
    memset(buf, 0, sb.s_blksz);

    /* FIXME:  0x20 should be sb.direntsz.  */
    /* FIXME:  4 should be sizeof(__ino_t).  */

    *((__ino_t *)((char *)buf + 0x00)) = 1;
    *((__ino_t *)((char *)buf + 0x20)) = 1;
    *((char *)buf + 0		  + 4) = '.';	/* directory "."  */
    *((char *)buf + sb.s_direntsz + 4) = '.';	/* directory ".." */
    *((char *)buf + sb.s_direntsz + 4 + 1) = '.';
    lseek(handle, first_data_block * sb.s_blksz, SEEK_SET);
    __write__(handle, buf, sb.s_blksz);

    return 0;
}
