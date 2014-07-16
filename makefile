MACHINE = $(shell uname -m)
ifeq ($(MACHINE), armv5tel)
  CC = gcc -g 
else
  CC = gcc -g #-Wl,--hash-style=both
endif

OBJS = pcomm.o simclist.o

CFLAGS = -Wall -I..

all: libpcomm.a

pcomm.o: pcomm.c pcomm.h simclist.h
simclist.o: simclist.c simclist.h

libpcomm.a: $(OBJS)
	ar -r $@ $(OBJS)

clean:
	/bin/rm -f *.o libpcomm.a

