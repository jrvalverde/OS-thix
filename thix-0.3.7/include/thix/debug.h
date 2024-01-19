#ifndef _THIX_DEBUG_H
#define _THIX_DEBUG_H


#ifdef __KERNEL__
#include <thix/string.h>
#endif


/*
 * Thix  distribution kernels should   be compiled without -D__DEBUG__ as
 * DEBUG() is called *BEFORE* checking the validity of some system call's
 * parameters.   I  have tried  to  delay the  DEBUG()   call as much  as
 * possible but sometimes displaying  it at the  end  of the function  is
 * useless or even misleading.  For example, filename validity checks are
 * performed in the namei() function.  Calling DEBUG() after namei() will
 * make debugging much more difficult - if something fails in the  middle
 * of namei() we will not be able to tell who called it.
 */


/*
 *  Kernel debug levels: (0-9)
 *
 *  0. reserved for the current debugging session
 *  1. status messages like "Clearing the free list ... "
 *  2. miscelaneous kernel status and error messages
 *  3. 
 *  4. most system calls - those ones that are not called too often
 *  5. all the system calls
 *  6. file system
 *  7. swapper
 *  8.
 *  9. device drivers informations (controller results, etc ...)
 */


/* Structure passed to the sys_kctrl() system call to dinamically
   configure the kernel.  This structure does not belong here!  */
struct kctrl
{
    char debug_levels[12];	/* string specifying the kernel debug
				   levels.  */
};


#ifdef __KERNEL__

#ifdef __DEBUG__

extern char thix_debug_levels[];

#define DEBUG(debug_level, format, args...)                             \
{                                                                       \
    if (strchr(thix_debug_levels, '0' + debug_level))                   \
    {                                                                   \
	printk("[%d] %d:%s: ", this->pid, __LINE__, __FUNCTION__);	\
	printk(format, ## args);                                        \
    }                                                                   \
}
#else
#define DEBUG(debug_level, format, args...) /**/
#endif /* __DEBUG__ */

#endif /* __KERNEL__ */


#endif  /* _THIX_DEBUG_H */
