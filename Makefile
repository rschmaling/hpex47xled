SHELL = /usr/local/bin/bash

# compiler and flags
CC = gcc
CXX = g++
# FLAGS = -Wall -O2 -fvariable-expansion-in-unroller -ftree-loop-ivcanon -funroll-loops -fexpensive-optimizations -fomit-frame-pointer
FLAGS = -Wall -O2 
CFLAGS = $(FLAGS)
CXXFLAGS = $(CFLAGS)
LDFLAGS = -lcam -ldevstat 


# build libraries and options

all: clean hpex47xled.o hpex47xled

hpex47xled.o: hpex47xled.c
	$(CC) $(CFLAGS) -o $@ -c $^

hpex47xled: hpex47xled.o
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

.PHONY: clean

clean:
	rm -f *.o hpex47xled *.core 
