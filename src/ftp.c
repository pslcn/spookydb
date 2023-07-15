#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <poll.h>
#include <fcntl.h> 
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>

#include "nb_fd_io.h"

#define LENGTH(X) (sizeof X / sizeof X[0])

#define BUFFSIZE 1024
#define NUM_CONNECTIONS 32

typedef struct {
	int serv_fd20, serv_fd21;
	struct sockaddr_in servaddr_20, servaddr_21;

	struct pollfd *poll_21fds, *poll_20fds;
	size_t poll_21fds_size, poll_20fds_size;

	fd_buff_struct_t *fds21_buffs, *fds20_buffs;
	size_t fds21_buffs_size, fds20_buffs_size;
} ftp_handler_t;

int init_ftp_handler(ftp_handler_t *ftp_handler)
{
	if (create_serv_sock(&(ftp_handler->serv_fd20), &(ftp_handler->servaddr_20), 20) != 0)
		return 1;
	if (create_serv_sock(&(ftp_handler->serv_fd21), &(ftp_handler->servaddr_21), 21) != 0)
		return 1;

	if (listen(ftp_handler->serv_fd20, (int)(NUM_CONNECTIONS / 2)) < 0)
		return 1;
	if (listen(ftp_handler->serv_fd21, (int)(NUM_CONNECTIONS / 2)) < 0)
		return 1;

	ftp_handler->poll_21fds_size = NUM_CONNECTIONS;
	ftp_handler->poll_20fds_size = NUM_CONNECTIONS;
	ftp_handler->poll_21fds = malloc(sizeof(struct pollfd) * ftp_handler->poll_21fds_size);
	ftp_handler->poll_20fds = malloc(sizeof(struct pollfd) * ftp_handler->poll_20fds_size);

	ftp_handler->poll_21fds[0].fd = ftp_handler->serv_fd21;
	ftp_handler->poll_21fds[0].events = POLLIN;

	ftp_handler->fds20_buffs_size = ftp_handler->poll_20fds_size;
	ftp_handler->fds21_buffs_size = ftp_handler->poll_21fds_size;
	ftp_handler->fds20_buffs = malloc(sizeof(fd_buff_struct_t) * ftp_handler->fds20_buffs_size);
	ftp_handler->fds21_buffs = malloc(sizeof(fd_buff_struct_t) * ftp_handler->fds21_buffs_size);

	return 0;
}

int close_ftp_handler(ftp_handler_t *ftp_handler)
{
	if (ftp_handler == NULL)
		return 1;

	for (size_t i = 0; i < NUM_CONNECTIONS; ++i) {
		if (ftp_handler->poll_21fds[i].fd > 0)
			close(ftp_handler->poll_21fds[i].fd);
	}
	free(ftp_handler->poll_21fds);

	for (size_t i = 0; i < NUM_CONNECTIONS; ++i) {
		if (ftp_handler->poll_20fds[i].fd > 0)
			close(ftp_handler->poll_20fds[i].fd);
	}
	free(ftp_handler->poll_20fds);

	free(ftp_handler->fds21_buffs);
	free(ftp_handler->fds20_buffs);

	return 0;
}

/* Testing */
#if 1
int main(void)
{
	ftp_handler_t ftp_handler;

	init_ftp_handler(&ftp_handler);
	
	close_ftp_handler(&ftp_handler);

	return 0;
}
#endif
