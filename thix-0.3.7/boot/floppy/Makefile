###
### Makefile for the boot/floppy subdirectory.
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


AS86    = as86 -0 -a
LD86    = ld86 -0

OBJS    = boot.o

.SUFFIXES: .img .bin

.S.o:
	$(AS86) -o $*.o $*.S

.o.img:
	$(LD86) -s -o $*.img $*.o

.img.bin:
	dd if=$*.img of=$*.bin bs=32 skip=1

boot.bin:
	dd if=boot.img of=boot.bin bs=32 skip=1

boot.img: boot.o

boot.bin: boot.img

install:
	echo "Do it by hand, I don't want to take any chances !!!"
	echo "'dd if=boot.bin of=/dev/fd0' is probably what you want."

tags:
	etags *.S

clean:
	rm -f core.* *.o *.img *.bin *~
