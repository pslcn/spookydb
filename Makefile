all: src/db.c src/non_blocking_fd_io.c src/ftp.c
	gcc -Wall -Wextra src/db.c src/non_blocking_fd_io.c src/ftp.c -o bin/db
	./test/root_build.sh bin/db

# test: src/db.c src/non_blocking_fd_io.c
# 	gcc -Wall -Wextra -g src/db.c src/non_blocking_fd_io.c -o bin/db 
