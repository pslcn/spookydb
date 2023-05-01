#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <sys/time.h>
#include <pthread.h>

#define BUFFSIZE 1024
#define LENGTH(X) (sizeof X / sizeof X[0])

typedef struct {
	int fd;
	char chunk_buff[BUFFSIZE];
	int chunk_buff_len;
	int chunk_buff_used; /* Could use to represent state */
	int is_sending;
} file_conn_t;

int fd_set_nb(int fd) /* Set FD to non-blocking */
{
	int flags;
#if defined(O_NONBLOCK)
	if ((flags = fcntl(fd, F_GETFL, 0)) == -1)
		flags = 0;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else 
	flags = 1;
	return ioctl(fd, FIOBIO, &flags);
#endif
}

void send_file(char *filename, int conn_fd, file_conn_t *file_conn)
{
	file_conn->fd = open(filename, O_RDONLY);
	if (file_conn->fd < 0)
		exit(1); /* Open failed */
	file_conn->chunk_buff_len = 0;
	file_conn->chunk_buff_used = 0;
	file_conn->is_sending = 1;
}

int handle_io(file_conn_t *file_conn)
{
	int nwrite;

	if (file_conn->is_sending == 0)
		return 2;

	if (file_conn->chunk_buff_used == file_conn->chunk_buff_len) {
		file_conn->chunk_buff_len = read(file_conn->fd, file_conn->chunk_buff, BUFFSIZE);
		if (file_conn->chunk_buff_len == 0) { /* Finished reading file */
			close(file_conn->fd);
			close(conn_fd);
			file_conn->is_sending = 0;
			return 1;
		}
		file_conn->chunk_buff_used = 0;
	}

	assert(file_conn->chunk_buff_len > file_conn->chunk_buff_used);
	nwrite = write(conn_fd, file_conn->chunk_buff + file_conn->chunk_buff_used, file_conn->chunk_buff_len - file_conn->chunk_buff_used);
	if (nwrite < 0)
		exit(1); /* Write failed */
	file_conn->chunk_buff_used += nwrite;

	return 0;
}

int main(void)
{
	int sockfd;
	return 0;
}
