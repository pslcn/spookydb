all: src/server.c src/ukv.c
	gcc -Iinclude src/server.c src/ukv.c -o ukv-server
