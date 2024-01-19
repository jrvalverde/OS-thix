CC      = 	cc
CPP     = 	$(CC) -E
LD      = 	ld
STRIP   = 	size

CFLAGS  =	-ggdb -Wall
LDFLAGS =

all:    tfsck.c tfsck mktfs.c mktfs
	@echo	done.

.c.o:
	$(CC) -c $(CFLAGS) $*.c

tfsck:	tfsck.o
	$(CC) tfsck.o -o tfsck
	$(STRIP) tfsck

mktfs:	mktfs.o
	$(CC) mktfs.o -o mktfs
	$(STRIP) mktfs

dep:
	sed '/\#\#\# Dependencies/q' <Makefile >tmp_make
	(for n in *.c; do $(CPP) -MM $$n; done) >>tmp_make
	mv tmp_make Makefile

tags:
	etags *.c
clean:	
	rm -f *.o *~ core* tfsck mktfs

### Dependencies
mktfs.o: mktfs.c ../thix/include/thix/fs.h \
 ../thix/include/thix/inode.h
tfsck.o: tfsck.c ../thix/include/thix/fs.h \
 ../thix/include/thix/cluster.h ../thix/include/thix/inode.h
