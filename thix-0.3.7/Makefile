###
### Main Makefile for the Thix 0.3 Operating System
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

CC       = cc
AS       = as
LD       = ld
LS       = ls
RM       = rm
AWK      = awk
CAT      = cat
SIZE     = size
SORT     = sort
STRIP    = strip
HDWRITE  = hdwrite
OBJDUMP  = objdump
TAR	 = tar
GZIP	 = gzip
BASENAME = basename

PROC     = i386
ARCH     = pc

VERSION  = 0.3.7

ROOTDIR := $(shell if [ "$$PWD" != "" ]; then echo $$PWD; else pwd; fi)

INCLUDE = $(ROOTDIR)/include
export INCLUDE

SUBDIRS = doc lib vm kernel proc/$(PROC) arch/$(ARCH) fs \
	  driver/block driver/character
BOOT_SUBDIRS = boot/floppy boot/harddisk

###
### __KERNEL__      - Used in include files to specify that we are compiling
###                   the kernel.
### __DEBUG__       - Display information about system calls parameters.
### __PARANOIA__    - Perform consistency checkings.
### __VTTY50L__     - 50 lines VGA mode (instead of the default 25).
### __i386k__       - Build a kernel that works on i386s.
###

###
### WARNING:
###
### Thix  distribution kernels should   be compiled without -D__DEBUG__ as
### DEBUG() is called *BEFORE* checking the validity of some system call's
### parameters.   I  have tried  to  delay the  DEBUG()   call as much  as
### possible but sometimes displaying  it at the  end  of the function  is
### useless or even misleading.  For example, filename validity checks are
### performed in the namei() function.  Calling DEBUG() after namei() will
### make debugging much more difficult - if something fails in the  middle
### of namei() we will not be able to tell who called it.
###


CFLAGS  = -Wall -m486 -O2 -fomit-frame-pointer -pipe -b i486-linuxaout\
	  -nostdinc -I$(INCLUDE) -DVERSION=\"$(VERSION)\"\
	  -D__KERNEL__ -D__DEBUG__ -D__PARANOIA__

export CFLAGS

LDFLAGS = -nostdlib -m i386linux

export LDFLAGS

OBJS    = proc/i386/i386.o arch/pc/pc.o kernel/kernel.o vm/vm.o lib/lib.o\
	  fs/fs.o driver/block/block.o driver/character/character.o

all:
	@(\
	    echo "Compiling the Thix $(VERSION) Operating System kernel ... ";\
	    for i in $(SUBDIRS); do (cd $$i && $(MAKE)) || exit; done;\
	    echo "done.";\
	    \
	    echo  -n "Linking ... ";\
	    $(CC) $(LDFLAGS) $(OBJS) -o thix;\
	    \
	    if test $$? -ne 0; then\
	        exit 1;\
	    fi;\
	    \
	    echo "done";\
	    \
	    echo   -n "Generating the kernel info ... ";\
	    $(OBJDUMP) --syms thix > thix.db;\
	    echo  "done";\
	    \
	    echo   -n "Sorting the kernel info ... ";\
	    $(AWK) '{  if (l < 4)\
			print > "thix.db.head";\
		    else\
			print > "thix.db.tail";\
		    l++ }' thix.db;\
	    \
	    $(SORT) +0 -o thix.db.tail.sorted < thix.db.tail;\
	    $(RM) thix.db thix.db.tail;\
	    $(CAT) thix.db.head thix.db.tail.sorted > thix.db;\
	    $(RM)  thix.db.head thix.db.tail.sorted;\
	    echo "done";\
	    \
	    echo -n "Preserving an unstriped version of the kernel ... ";\
	    cp thix thix.unstriped;\
	    echo "done";\
	    \
	    echo -n "Striping the kernel ... ";\
	    $(STRIP) thix;\
	    echo "done";\
	    \
	    chmod 644 thix;\
	    \
	    echo "Kernel size:";\
	    $(SIZE) thix;\
	    $(LS) -l thix;\
	    echo "Kernel headers:";\
	    $(OBJDUMP) --file-headers --headers thix;\
	    \
	    echo -n "Writing the kernel to disk ... ";\
	    echo "done";\
	)

doc:    ps

info:
	@(\
	    echo "Making info documentation ... ";\
	    (cd doc && $(MAKE) $@);\
	    echo "done.";\
	)

dvi:
	@(\
	    echo "Making dvi documentation ... ";\
	    (cd doc && $(MAKE) $@);\
	    echo "done.";\
	)

ps:
	@(\
	    echo "Making PostScript documentation ... ";\
	    (cd doc && $(MAKE) $@);\
	    echo "done.";\
	)

tags:
	@(\
	    echo "Making tags ... ";\
	    etags vm/*.c kernel/*.c fs/*.c lib/*.c\
		  driver/block/*.c driver/character/*.c\
		  proc/i386/*.c proc/i386/*.S arch/pc/*.c\
		  include/thix/*.h;\
	    echo "done.";\
	)

dep:
	@(\
	    echo "Making dependencies ... ";\
	    for i in $(SUBDIRS); do (cd $$i && $(MAKE) $@) || exit; done;\
	    echo "done.";\
	)

clean:
	@(\
	    echo "Cleaning ... ";\
	    for i in $(SUBDIRS); do (cd $$i && $(MAKE) $@); done;\
	    for i in $(BOOT_SUBDIRS); do (cd $$i && $(MAKE) $@); done;\
	    $(RM) -f core* thix thix.unstriped thix.db *~;\
	    $(RM) -f thix.db.head thix.db.tail thix.db.tail.sorted;\
	    $(RM) -f include/*~;\
	    echo "done.";\
	)

dist:	clean
	@(\
	    dir=`pwd` && cd ..;\
	    $(TAR) cf - `$(BASENAME) $$dir` |\
	    $(GZIP) -9c > `$(BASENAME) $$dir`.tar.gz;\
	    \
	    if test $$? != 0; then\
		exit 1;\
	    fi;\
	)
