all: src/db.c src/net_threads.c
	gcc src/db.c src/net_threads.c -o bin/db
