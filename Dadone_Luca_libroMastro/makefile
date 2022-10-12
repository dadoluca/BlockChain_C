CC=gcc
CFLAGS=-std=c99 -pedantic -D_GNU_SOURCE
#-Wall -Werror -std=c89 -pedantic -D_GNU_SOURCE
#-std=c89 -pedantic

all: master node user

master: master.c common.o
	$(CC) $(CFLAGS) master.c common.o -o master

node: node.c common.o
	$(CC) $(CFLAGS) node.c common.o -o node

user: user.c common.o
	$(CC) $(CFLAGS) user.c common.o -o user

common.o: common.c
	$(CC) $(CFLAGS) common.c -c

clean:
	rm -f *.o master node user *~