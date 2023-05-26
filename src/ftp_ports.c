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
	int fd; 

	size_t rbuff_size;
	uint8_t rbuff[BUFFSIZE];
} port_buff_struct_t;

typedef struct {
	int serv_fd_20, serv_fd_21;
	struct sockaddr_in servaddr_20, servaddr_21;

	struct pollfd **port_21_fds;
	struct pollfd **port_20_fds;
	size_t port_21_fds_size;
	size_t port_20_fds_size;

	port_buff_struct_t *port_21_buffs;
	port_buff_struct_t *port_20_buffs;
	size_t port_21_buffs_size;
	size_t port_20_buffs_size;
} ftp_ports_t;

/* Concurrency management with poll.h; port 20 and port 21 are served on separate threads */
int create_ftp_handler(ftp_ports_t **ftp_handler)
{
	*ftp_handler = malloc(sizeof(ftp_ports_t));
	fprintf(stdout, "%p: ftp_handler \n", *ftp_handler);

	/* Communication on port 21; data transfer on port 20 */
	fprintf(stdout, "%p: port 20 socket\n", &((*ftp_handler)->serv_fd_20));
	if (create_serv_sock(&((*ftp_handler)->serv_fd_20), &((*ftp_handler)->servaddr_20), 20) != 0)
		return 1;

	fprintf(stdout, "%p: port 21 socket\n", &((*ftp_handler)->serv_fd_21));
	if (create_serv_sock(&((*ftp_handler)->serv_fd_21), &((*ftp_handler)->servaddr_21), 21) != 0)
		return 1;

	if (listen((*ftp_handler)->serv_fd_20, (int)(NUM_CONNECTIONS / 2)) < 0)
		return 1;
	if (listen((*ftp_handler)->serv_fd_21, (int)(NUM_CONNECTIONS / 2)) < 0)
		return 1;

	(*ftp_handler)->port_21_fds_size = NUM_CONNECTIONS;
	(*ftp_handler)->port_20_fds_size = NUM_CONNECTIONS;

	(*ftp_handler)->port_21_fds = calloc((*ftp_handler)->port_21_fds_size, sizeof(struct pollfd *));
	(*ftp_handler)->port_20_fds = calloc((*ftp_handler)->port_20_fds_size, sizeof(struct pollfd *));
	fprintf(stdout, "%p: port_21_fds with size %d\n", (*ftp_handler)->port_21_fds, (*ftp_handler)->port_21_fds_size);
	fprintf(stdout, "%p: port_20_fds with size %d\n", (*ftp_handler)->port_20_fds, (*ftp_handler)->port_20_fds_size);

	for (size_t i = 0; i < (*ftp_handler)->port_21_fds_size; ++i) 
		(*ftp_handler)->port_21_fds[i] = malloc(sizeof(struct pollfd));
	for (size_t i = 0; i < (*ftp_handler)->port_20_fds_size; ++i) 
		(*ftp_handler)->port_20_fds[i] = malloc(sizeof(struct pollfd));

	(*ftp_handler)->port_21_fds[0]->fd = (*ftp_handler)->serv_fd_21;
	(*ftp_handler)->port_21_fds[0]->events = POLLIN;
	fprintf(stdout, "%p: serv_fd_21 (stored in port_21_fds)\n", (*ftp_handler)->port_21_fds[0]);

	(*ftp_handler)->port_20_buffs_size = (*ftp_handler)->port_21_fds_size;
	(*ftp_handler)->port_21_buffs_size = (*ftp_handler)->port_20_fds_size;
	(*ftp_handler)->port_20_buffs = malloc(sizeof(port_buff_struct_t) * (*ftp_handler)->port_20_buffs_size);
	(*ftp_handler)->port_21_buffs = malloc(sizeof(port_buff_struct_t) * (*ftp_handler)->port_21_buffs_size);

	return 0;
}

int close_ftp_handler(ftp_ports_t *ftp_handler)
{
	if (ftp_handler == NULL)
		return 1;

	fprintf(stdout, "Closing ftp_handler at %p\n", ftp_handler);

	if (ftp_handler->port_21_fds != NULL) {
		fprintf(stdout, "(close_ftp_handler) port_21_fds are at %p\n", ftp_handler->port_21_fds);

		for (size_t i = 0; i < NUM_CONNECTIONS; ++i) {
			if (ftp_handler->port_21_fds[i] == NULL)
				continue;

			/*
			fprintf(stdout, "ftp_handler->port_21_fds[%d]: %d at %p\n", i, ftp_handler->port_21_fds[i]->fd, ftp_handler->port_21_fds[i]); 
			*/

			if (ftp_handler->port_21_fds[i]->fd > 0) 
				close(ftp_handler->port_21_fds[i]->fd);

			free(ftp_handler->port_21_fds[i]);	
		}

		free(ftp_handler->port_21_fds);
	}

	if (ftp_handler->port_20_fds != NULL) {
		fprintf(stdout, "(close_ftp_handler) port_20_fds are at %p\n", ftp_handler->port_20_fds);

		for (size_t i = 0; i < NUM_CONNECTIONS; ++i) {
			if (ftp_handler->port_20_fds[i] == NULL)
				continue;

			if (ftp_handler->port_20_fds[i]->fd > 0)
				close(ftp_handler->port_20_fds[i]->fd);

			free(ftp_handler->port_20_fds[i]);
		}

		free(ftp_handler->port_20_fds);
	}

	if (ftp_handler->port_21_buffs != NULL)
		free(ftp_handler->port_21_buffs);
	if (ftp_handler->port_20_buffs != NULL)
		free(ftp_handler->port_20_buffs);

	free(ftp_handler);

	return 0;
}

/* Concurrency management with poll.h */
int ftp_poll_ports(ftp_ports_t *ftp_handler)
{
	size_t nfds = 1;

	/*
	for (size_t i = 1; i < ftp_handler->port_21_buffs_size; ++i) {
		if (ftp_handler->port_21_buffs[i - 1].fd >= 0) {
			ftp_handler->port_21_fds[i].fd = ftp_handler->port_21_buffs[i - 1].fd;
			ftp_handler->port_21_fds[i].events = (ftp_handler->port_21_buffs[i - 1].state == STATE_REQ) ? POLLIN : POLLOUT;
			ftp_handler->port_21_fds[i].events |= POLLERR;
			ftp_handler->port_21_fds[i].revents = 0;

			nfds += 1;
		}
	}
	*/

	return 0;
}

/* Testing */
#if 1
int main(void)
{
	ftp_ports_t *ftp_handler;

	create_ftp_handler(&ftp_handler);

	/*
	while (1) {
		fprintf(stdout, "ftp_poll_ports: %d\n", ftp_poll_ports(&ftp_handler));
		break;
	}
	*/

	close_ftp_handler(ftp_handler);

	return 0;
}
#endif

