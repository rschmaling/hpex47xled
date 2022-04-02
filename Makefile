SHELL = /usr/local/bin/bash

# compiler and flags
CC = cc
CXX = g++

RCPREFIX = /usr/local/etc/rc.d
PREFIX = /usr/local
RCFILE = hpex47xled.rc
# FLAGS = -Wall -O2 -fvariable-expansion-in-unroller -ftree-loop-ivcanon -funroll-loops -fexpensive-optimizations -fomit-frame-pointer
FLAGS = -O2 -Wall -Werror -std=gnu99 -march=native 
CFLAGS = $(FLAGS)
CXXFLAGS = $(CFLAGS)
LDFLAGS = -lcam -ldevstat 
CFILES = hpex47xled.c
OBJS = hpex47xled.o
TARGETS = hpex47xled


# build libraries and options

all: clean hpex47xled.o hpex47xled

${OBJS}: ${CFILES}
	${CC} ${CFLAGS} ${INCLUDES} -c $?

${TARGETS}: ${OBJS}
	${CC} -o $@ $? ${CFLAGS} ${LDFLAGS}

.PHONY: clean

clean:
	rm -f *.o hpex47xled *.core 

.PHONY: install

install: 
	test -f $(RCPREFIX)/hpex49xled || install -m 755 $(RCFILE) $(RCPREFIX)/hpex49xled
	install -m 700 $(TARGETS) $(PREFIX)/bin/
	strip $(PREFIX)/bin/$(TARGETS)
