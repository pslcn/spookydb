all: src/db.c src/non_blocking_server.c src/http.c src/hashtable.c
	gcc -Wall -Wextra src/db.c src/non_blocking_server.c src/http.c src/hashtable.c -o bin/db

http: src/http.c src/non_blocking_server.c
	gcc -Wall -Wextra src/http.c src/non_blocking_server.c -o bin/http
