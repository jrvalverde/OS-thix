###
### Makefile for the doc subdirectory.
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


all:

dep:

info: thix.info

thix.info: thix.texinfo
	makeinfo thix.texinfo

dvi: thix.dvi

thix.dvi: thix.texinfo thix.info
	texi2dvi thix.texinfo

ps: thix.ps

thix.ps: thix.dvi
	dvips thix.dvi > thix.ps

tags:

clean:
	rm -f *.aux *.log *.ps *.dvi *.info* .depend *~ core.*
	rm -f *.fn *.cp *.vr *.ky *.pg *.tp *.toc
