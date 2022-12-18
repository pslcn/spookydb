all: src/server.c
	gcc -Iinclude src/server.c -o server
