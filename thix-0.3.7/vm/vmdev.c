/* vmdev.c -- Virtual Memory device number allocation.  */

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
#include <thix/string.h>
#include <thix/vmdev.h>


vmdev_struct vmdevt[MAX_VM_DEVICES];


/*
 * The kernel uses 16-bit device numbers:  8 bits for the major number and 8
 * bits for the minor number.  Unfortunately the number of bits available in
 * the page tables is limited (5).  In order to get arround the problem  the
 * kernel dynamically allocates a smaller device number (vmdev) just for the
 * use of the memory management routines each time  a  mount()  or  swapon()
 * operation is performed.  Virtual memory routines  will  use  this  number
 * when swapping pages of memory.  Before doing the I/O operation, the vmdev
 * number is converted back to the real device number.  When the  device  is
 * no longer used (as a result of an umount() or swapoff() system call), the
 * vmdev number is released and can be reused.  This way we are  limited  to
 * 2^5 = 32 mounted+swap devices.  I think is good enough right now.
 */

/*
 *   Allocates a new VM device number for the major/minor device number if
 * the requested device is currently unused. This function returns  -1  if
 * there are no VM device numbers available.
 */

int
get_vmdev(__dev_t device, char type, int aux)
{
    int vmdev;

    for (vmdev = 0; vmdev < MAX_VM_DEVICES; vmdev++)
	if (vmdevt[vmdev].in_use == 0)
	{
	    vmdevt[vmdev].device = device;
	    vmdevt[vmdev].type   = type;
	    vmdevt[vmdev].in_use = 1;
	    vmdevt[vmdev].aux    = aux;
	    return vmdev;
	}
	else
	    if (vmdevt[vmdev].device == device)
	    {
		DEBUG(1, "**** DEVICE %w ALREADY IN USE ****\n", device);
		return -1;
	    }

    /* mount() / swapon() should return error if this will ever hapen.  */
    DEBUG(2, "no more VM device numbers");
    return -1;
}


/*
 * Releases the VM device number.
 */

void
release_vmdev(int vmdev)
{
#ifdef __PARANOIA__
    if (vmdevt[vmdev].in_use == 0)
	PANIC("**** Trying to release unused VM device number %x ****\n",
	      vmdev);
#endif

    vmdevt[vmdev].in_use = 0;
}


/*
 * Initialize the VM device stuff.  Fill the VM device table with zeros.
 */

void
vmdev_init(void)
{
    lmemset(vmdevt, 0, sizeof(vmdevt) >> 2);
}
