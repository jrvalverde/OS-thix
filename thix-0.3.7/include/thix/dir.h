#ifndef _THIX_DIR_H
#define _THIX_DIR_H


#ifndef _THIX_FS_H
#include <thix/fs.h>
#endif


int dir_open(int fd);
int dir_close(int fd);

int dir_read(int fd, char *buf, size_t count);
int dir_write(int fd, char *buf, size_t count);


int find_empty_dir_slot(superblock *sb, int dir_i_no, unsigned dir_offset);
int setup_directory_entry(superblock *sb, int dir_i_no, int offset,
					  int i_no, char *entry_name);
int is_dir_empty(superblock *sb, int dir_i_no);


#endif  /* _THIX_DIR_H */
