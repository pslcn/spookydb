all: src/db.c src/nb_fd_io.c
	gcc -Wall -Wextra src/db.c src/nb_fd_io.c -o bin/db

test: src/db.c src/nb_fd_io.c
	gcc -Wall -Wextra -g src/db.c src/nb_fd_io.c -o bin/db 

test_ftp: src/ftp_ports.c src/nb_fd_io.c
	gcc src/ftp_ports.c src/nb_fd_io.c -o bin/ftp
	./test/root_build.sh bin/ftp
