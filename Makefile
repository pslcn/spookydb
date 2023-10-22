all: src/db.c src/non_blocking_fd_io.c src/ftp.c src/hashtable.c
	gcc -Wall -Wextra src/db.c src/non_blocking_fd_io.c src/ftp.c src/hashtable.c -o bin/db
	./test/root_build.sh bin/db

http: src/http.c src/non_blocking_fd_io.c
	gcc -Wall -Wextra src/http.c src/non_blocking_fd_io.c -o bin/http

# test: src/db.c src/non_blocking_fd_io.c
# 	gcc -Wall -Wextra -g src/db.c src/non_blocking_fd_io.c -o bin/db 
