###
### Voof Makefile
###

ALL=port.h compress.h lzrw1a.c lzrw3a.c voof.c *EADME ?akefile

# Compiler to use; usually cc.
CC?=cc

# The usual flags for CC. -O for optimized code, -g for debug symbols...  
FLAGS=-O
#FLAGS=-g

# Define _SYSTYPE_SYSV if you are running UNIX System V, and your compiler
# won't predefine it for you.  If you know this machine has big endian
# byte ordering, you can also define BIG_ENDIAN for a tiny performance
# boost.
#DEFS=-D_SYSTYPE_SYSV -DBIG_ENDIAN
DEFS=

all: rw3a rw1a
	@echo 'Done.'

rw3a: lzrw3a.o voof.o
	$(CC) lzrw3a.o voof.o -o voof
	-@rm -f foov vcat unvoof
	-ln voof foov
	-ln voof vcat
#	-ln voof unvoof

rw1a: lzrw1a.o voof.o
	$(CC) lzrw1a.o voof.o -o voof1a

voof.o: port.h compress.h
	$(CC) $(FLAGS) $(DEFS) -c voof.c

lzrw3a.o: port.h compress.h
	$(CC) $(FLAGS) $(DEFS) -c lzrw3a.c

lzrw1a.o: port.h compress.h
	$(CC) $(FLAGS) $(DEFS) -c lzrw1a.c

tar:
	tar cvf - $(ALL) | compress -f > voof.tar.Z

shar:
	shar $(ALL) > voof.shar

tags:
	ctags -x *.[ch] > tags

clean:
	rm -f core *.o

distclean: clean
	rm -f voof foov vcat voof1a

clobber: clean
	rm -i $(ALL)  voof foov vcat voof1a

# eof
