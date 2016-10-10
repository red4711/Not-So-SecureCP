CC=gcc
CFLAGS=-std=gnu99 -Wall -ggdb 

PROGRAMS=cp_client cp_server
OBJECTS=${PROGRAMS:=.o}

all: cp_client cp_server

cp_client: cp_client.o
	$(CC) $(CFLAGS) $? -o $@

cp_server: cp_server.o
	$(CC) $(CFLAGS) $? -o $@

.c.o:
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJECTS)

strip:
	strip -S $(PROGRAMS)
