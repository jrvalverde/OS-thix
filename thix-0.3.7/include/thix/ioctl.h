#ifndef _THIX_IOCTL_H
#define _THIX_IOCTL_H


#define TIOCGWINSZ      0x00005413   /* Get window size.  */
#define TIOCSWINSZ      0x00005414   /* Set window size.  */

/* Thix stuff. */
#define IOCTL_GETBLKSZ  0x00005480   /* Get device block size.  */
#define IOCTL_GETDEVSZ  0x00005481   /* Get device size (in no. of blocks).  */

/* Sun compatibility. */
#define TIOCGSIZE       TIOCGWINSZ
#define TIOCSSIZE       TIOCSWINSZ


struct winsize
{
  unsigned short int ws_row;    /* Rows, in characters.  */
  unsigned short int ws_col;    /* Columns, in characters.  */

  /* These are not actually used.  */
  unsigned short int ws_xpixel; /* Horizontal pixels.  */
  unsigned short int ws_ypixel; /* Vertical pixels.  */
};


#endif /* _THIX_IOCTL_H */
