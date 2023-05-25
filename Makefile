all: src/db.c src/nb_fd_io.c src/spkdb_http.c 
	gcc src/db.c src/nb_fd_io.c src/spkdb_http.c -o bin/db

test_ftp: src/ftp_ports.c src/nb_fd_io.c
	gcc src/ftp_ports.c src/nb_fd_io.c -o bin/ftp
	./test/root_build.sh bin/ftp
