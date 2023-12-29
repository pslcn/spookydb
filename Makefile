CC = cc
CFLAGS = -std=c99 -pedantic -Wall -Wno-deprecated-declarations

SRC = src/non_blocking_server.c src/http.c src/hashtable.c src/db.c

all:
	${CC} ${CFLAGS} -o bin/db ${SRC} 

httpserver: 
	${CC} ${CFLAGS} -o bin/httpserver src/non_blocking_server.c src/spkdb_http.c test/httpserver.c
