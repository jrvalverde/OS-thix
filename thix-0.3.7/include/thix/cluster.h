#ifndef _THIX_CLUSTER_H
#define _THIX_CLUSTER_H


#ifndef _THIX_FS_H
#include <thix/fs.h>
#endif


/*
 * A macro that gives the number of data blocks in a given cluster.
 */

#define cluster_data_blocks(cluster)                                     \
    (((cluster == sb->s_clusters - 1) && sb->s_lcflag) ? sb->s_blcluster:\
							 sb->s_bcluster)


/*
 * A macro that converts a cluster reserved block number into a file system
 * block number. It performs no checks against out of range cluster blocks.
 */

#define cluster_reserved2fs(cluster, block)                             \
    (1 + 1 + sb->s_ssbldsz + sb->s_bblsz + cluster * sb->s_bpcluster + block)


/*
 * A macro that converts a cluster  data  block  number  into a  file system
 * block number.  It performs no checks against out of range cluster blocks.
 */

#define cluster2fs(cluster, block)                                      \
    (1 + 1 + sb->s_ssbldsz + sb->s_bblsz + cluster * sb->s_bpcluster +  \
     1 + 1 + 1 + ((sb->s_icluster * sizeof(dinode_t)) / sb->s_blksz) + block)


/*
 * A macro that converts a file system block number into a  cluster block
 * number. It performs no checks against out of range file system blocks.
 */

#define fs2cluster(block)                                                  \
    (((block - (1 + 1 + sb->s_ssbldsz + sb->s_bblsz)) % sb->s_bpcluster) - \
    (1 + 1 + 1 + ((sb->s_icluster * sizeof(dinode_t)) / sb->s_blksz)))


#ifdef __KERNEL__

int search_cluster(int mp, int type);
int switch_cluster(int mp, int cluster, int type, int force);
void flush_cluster(int mp, int type);

int get_bitmap_resource(unsigned *bitmap, int bitmap_size);
void release_bitmap_resource(unsigned *bitmap, int resource);

#define get_bitmap_inode get_bitmap_resource
#define get_bitmap_block get_bitmap_resource

#define release_bitmap_inode release_bitmap_resource
#define release_bitmap_block release_bitmap_resource

#endif  /* __KERNEL__ */


#endif  /* _THIX_CLUSTER_H */
