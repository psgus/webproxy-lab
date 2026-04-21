# Makefile for Proxy Lab 
#
# You may modify this file any way you like (except for the handin
# rule). You instructor will type "make" on your specific Makefile to
# build your proxy from sources.

CC = gcc
CFLAGS = -g -Wall
LDFLAGS = -lpthread

all: proxy echoserver echoclient hostinfo

csapp.o: csapp.c csapp.h
	$(CC) $(CFLAGS) -c csapp.c

echoclient.o: echoclient.c csapp.h
	$(CC) $(CFLAGS) -c echoclient.c

echoserver.o: echoserver.c csapp.h
	$(CC) $(CFLAGS) -c echoserver.c

proxy.o: proxy.c csapp.h
	$(CC) $(CFLAGS) -c proxy.c

hostinfo.o: hostinfo.c csapp.h
	$(CC) $(CFLAGS) -c hostinfo.c

hostinfo: hostinfo.o csapp.o
	$(CC) $(CFLAGS) hostinfo.o csapp.o -o hostinfo $(LDFLAGS)

echoclient: echoclient.o csapp.o
	$(CC) $(CFLAGS) echoclient.o csapp.o -o echoclient $(LDFLAGS)

echoserver: echoserver.o csapp.o
	$(CC) $(CFLAGS) echoserver.o csapp.o -o echoserver $(LDFLAGS)


proxy: proxy.o csapp.o
	$(CC) $(CFLAGS) proxy.o csapp.o -o proxy $(LDFLAGS)

# Creates a tarball in ../proxylab-handin.tar that you can then
# hand in. DO NOT MODIFY THIS!
handin:
	(make clean; cd ..; tar cvf $(USER)-proxylab-handin.tar proxylab-handout --exclude tiny --exclude nop-server.py --exclude proxy --exclude driver.sh --exclude port-for-user.pl --exclude free-port.sh --exclude ".*")

clean:
	rm -f *~ *.o proxy echoserver echoclient hostinfo core *.tar *.zip *.gzip *.bzip *.gz