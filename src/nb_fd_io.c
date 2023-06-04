/* Non-Blocking FD I/O */

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
#include <poll.h>
#include <fcntl.h> 
#include <arpa/inet.h>
#include <errno.h>

#include "nb_fd_io.h"

int create_fd_buff_struct(fd_buff_struct_t *fd_buff, size_t rbuff_size, size_t wbuff_capacity)
{
	fd_buff->fd = 0;

	fd_buff->rbuff_size = rbuff_size;	
	fd_buff->wbuff_capacity = wbuff_capacity;

	fd_buff->rbuff = calloc(fd_buff->rbuff_size, sizeof(uint8_t));
	fd_buff->wbuff = calloc(fd_buff->wbuff_capacity, sizeof(uint8_t));

	return 0;
}

int close_fd_buff_struct(fd_buff_struct_t *fd_buff)
{
	if (fd_buff == NULL)
		return 1;

	fprintf(stdout, "Closing fd_buff_struct_t at %p\n", fd_buff);

	if (fd_buff->rbuff != NULL) {
		for (size_t i = 0; i < fd_buff->rbuff_size; ++i) {
			if (fd_buff->rbuff == NULL)
				continue;

			free(fd_buff->rbuff);	
		}

		free(fd_buff->rbuff);
	}

	if (fd_buff->wbuff != NULL) {
		for (size_t i = 0; i < fd_buff->wbuff_capacity; ++i) {
			if (fd_buff->wbuff == NULL)
				continue;

			free(fd_buff->wbuff);
		}

		free(fd_buff->wbuff);
	}

	free(fd_buff);

	return 0;
}

int fd_set_non_blocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1)
		return 1;
	flags |= O_NONBLOCK;

	return (fcntl(fd, F_SETFL, flags) == 0) ? 0 : 1;
}

int create_serv_sock(int *serv_fd, struct sockaddr_in *servaddr, int port) 
{
	*serv_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (*serv_fd == -1)
		return 1;
	fd_set_non_blocking(*serv_fd);
	/* fprintf(stdout, "%p: created non-blocking socket\n", serv_fd); */
	
	int enable = 1;
	if (setsockopt(*serv_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
		return 1;

	bzero(servaddr, sizeof(*servaddr));
	(*servaddr).sin_family = AF_INET;
	(*servaddr).sin_addr.s_addr = htonl(INADDR_ANY);
	(*servaddr).sin_port = htons(port);

	if ((bind(*serv_fd, (struct sockaddr *)servaddr, sizeof(*servaddr))) != 0) {
		if (errno == EADDRINUSE)
			fprintf(stdout, "Address already in use\n");

		else if (errno == EACCES)
			fprintf(stdout, "EACCES error\n");

		return 1;
	}

	return 0;
}

void serv_accept_connection(int serv_fd, fd_buff_struct_t *fd_buff_structs, size_t fd_buff_structs_size) 
{
	int conn_fd = 0;

	do {
		struct sockaddr_in cli;
		int addrlen = sizeof(cli);
		conn_fd = accept(serv_fd, (struct sockaddr *)&cli, &addrlen);
		if (conn_fd < 0)
			break;
		fd_set_non_blocking(conn_fd);

		/* fprintf(stdout, "Accepted %s\n", inet_ntoa(cli.sin_addr)); */

		for (size_t i = 0; i < fd_buff_structs_size; ++i) {
			if (fd_buff_structs[i].fd <= 0) {
				fprintf(stdout, "%p: Storing FD %d in fd_buff_structs[%d]\n", &(fd_buff_structs[i].fd), conn_fd, i);

				fd_buff_structs[i].fd = conn_fd;
				fd_buff_structs[i].state = STATE_REQ;
				fd_buff_structs[i].rbuff_size = 0;
				fd_buff_structs[i].wbuff_size = 0;
				fd_buff_structs[i].wbuff_sent = 0;
				break;
			}
		}
	} while (conn_fd != -1);
}

int prepare_pollfd_array(fd_buff_struct_t *fd_buff_structs, struct pollfd *pollfd_array, size_t pollfd_array_size, size_t *nfds)
{
	*nfds = 1;

	for (size_t i = 1; i < pollfd_array_size; ++i) {
		if (fd_buff_structs[i - 1].fd > 0) {
			fprintf(stdout, "%p: Saving FD %d in pollfd_array[%d]\n", &pollfd_array[i], fd_buff_structs[i - 1].fd, i);

			pollfd_array[i].fd = fd_buff_structs[i - 1].fd;
			pollfd_array[i].events = (fd_buff_structs[i - 1].state == STATE_REQ) ? POLLIN : POLLOUT;
			pollfd_array[i].events |= POLLERR;
			pollfd_array[i].revents = 0;

			*nfds += 1;
		}
	}

	return 0;
}

