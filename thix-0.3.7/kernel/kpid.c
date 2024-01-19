/* kpid.c -- Kernel process ids management routines.  */

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
#include <thix/limits.h>
#include <thix/kpid.h>
#include <thix/process.h>
#include <thix/string.h>


int kpid_tail = INIT;

kpid_struct kpid_data[SYSTEM_PROCESS_MAX];

/* There are three running processes: IDLE, SWAPPER & INIT.  */
int kpids = 3;

/*
 * IDLE, SWAPPER and INIT will  always be in the  list.  It means that
 * the prev field will  always be valid.   Note that  the end of  list
 * test is performed by checking for 0, which means that we will never
 * be able to see IDLE this way.  Who cares....
 */


/*
 * Allocate a new kernel process id (kpid).  Returns 0 if there is no
 * free slot in the table.
 */

int
get_kpid(void)
{
    int kpid;

    /* The last two empty process slot can be used only if superuser.  */
    if (kpids >= SYSTEM_PROCESS_MAX - 2 && !superuser())
	return 0;

    for (kpid = FF_KPID; kpid < SYSTEM_PROCESS_MAX; kpid++)
	if (!kpid_data[kpid].used)
	{
	    kpid_data[kpid].used = 1;
	    kpid_data[kpid].next = 0;
	    kpid_data[kpid].prev = kpid_tail;
	    kpid_data[kpid_tail].next = kpid;
	    kpids++;
	    return kpid_tail = kpid;
	}

    /* The process slot table is full.  */
    return 0;
}


/*
 * Release a kernel process id.  Its entry is freed and can be reused
 * by another process.
 */

void
release_kpid(int kpid)
{
    kpid_data[kpid].used = 0;

    if (kpid == kpid_tail)
    {
	kpid_tail = kpid_data[kpid].prev;
	kpid_data[kpid_tail].next = 0;
    }
    else
    {
	kpid_data[kpid_data[kpid].prev].next = kpid_data[kpid].next;
	kpid_data[kpid_data[kpid].next].prev = kpid_data[kpid].prev;
    }

    kpids--;
}


/*
 * Initialize kernel process ids management.
 */

int
kpid_init(void)
{
    /* Initializa kpid_data.  */
    memset(kpid_data, 0, sizeof(kpid_data));

    /* Add IDLE, SWAPPER and INIT to the list.  */
    kpid_data[IDLE].used = 1;
    kpid_data[IDLE].next = SWAPPER;
    kpid_data[IDLE].prev = 0;

    kpid_data[SWAPPER].used = 1;
    kpid_data[SWAPPER].next = INIT;
    kpid_data[SWAPPER].prev = IDLE;

    kpid_data[INIT].used = 1;
    kpid_data[INIT].next = 0;
    kpid_data[INIT].prev = SWAPPER;

    return 1;
}
