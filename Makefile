CC = gcc
#CFLAGS = -W -Wall -Werror
CFLAGS = -W -Wall
	
server: web-server.o
	$(CC) -o server web-server.o
	
web-server.o: web-server.c
	$(CC) $(CFLAGS) -c web-server.c