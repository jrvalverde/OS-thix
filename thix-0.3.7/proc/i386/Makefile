###
### Makefile for the proc/i386 subdirectory.
###

### 
### Thix Operating System
### Copyright (C) 1993,1996 Tudor Hulubei
### 
### This program is  free software; you can  redistribute it and/or modify
### it under the terms  of the GNU General Public  License as published by
### the  Free Software Foundation; either version  2,  or (at your option)
### any later version.
### 
### This program is distributed  in the hope that  it will be  useful, but
### WITHOUT    ANY  WARRANTY;  without   even   the  implied  warranty  of
### MERCHANTABILITY  or  FITNESS FOR  A PARTICULAR   PURPOSE.  See the GNU
### General Public License for more details.
### 
### You should  have received a  copy of  the GNU  General Public  License
### along with   this  program; if   not,  write  to   the Free   Software
### Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
### 


.S.o:
	$(CC) $(CFLAGS) -c $<
.c.o:
	$(CC) $(CFLAGS) -c $<

OBJS  = startup.o async.o syscalls.o memcpy.o setup.o regs.o faults.o


i386.o: $(OBJS)
	$(LD) $(LDFLAGS) -r -o i386.o $(OBJS)

clean:
	rm -f core.* .depend *.o *~

dep:
	$(CPP) $(CFLAGS) -M *.c *.S > .depend

#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif

