CC = cc
CFLAGS = -std=c99 -pedantic -Wall -Wno-deprecated-declarations 

SRC = src/non_blocking_server.c src/http.c src/hashtable.c src/db.c

all:
	${CC} ${CFLAGS} ${SRC} -o bin/db
