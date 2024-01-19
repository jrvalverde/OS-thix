#ifndef _THIX_KPID_H
#define _THIX_KPID_H


/*
 * Process 0 --> idle
 * Process 1 --> swapper
 * Process 2 --> init
 */

#define IDLE            0       /* Idle process kpid.  */
#define SWAPPER         1       /* Swapper process kpid.  */
#define INIT            2       /* Init process kpid.  */
#define FF_KPID         3       /* First free kernel process id.  */


typedef struct
{
    signed char used;           /* Process slot in-use flag.  */
    signed char next;           /* Next process in the list.  */
    signed char prev;           /* Previous process in the list.  */
} kpid_struct;


extern kpid_struct kpid_data[];

extern int kpid_tail;


#define FIRST           (kpid_data[INIT].next)
#define NEXT(kpid)      (kpid_data[kpid].next)


int kpid_init(void);

int get_kpid(void);
void release_kpid(int kpid);


#endif  /* _THIX_KPID_H */
