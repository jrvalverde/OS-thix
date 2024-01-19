#ifndef _THIX_USTAT_H
#define _THIX_USTAT_H


struct ustat
{
    unsigned f_tfree;           /* Number of free blocks.  */
    unsigned f_tinode;          /* Number of free inodes.  */
    char     f_fname[6];        /* File system name.  */
    char     f_fpack[6];        /* File system pack name.  */
};


#endif  /* _THIX_USTAT_H */
