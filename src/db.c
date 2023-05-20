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

#define LENGTH(X) (sizeof X / sizeof X[0])

#define BUFFSIZE 1024
#define NUM_CONNECTIONS 32

enum {
	STATE_REQ = 0,
	STATE_RES = 1,
	STATE_END = 2,
};

typedef struct {
	int fd;
	uint32_t state;

	size_t rbuff_size;
	uint8_t rbuff[BUFFSIZE];

	size_t wbuff_size;
	size_t wbuff_sent;
	uint8_t wbuff[BUFFSIZE];
} fd_buff_struct_t;

static int fd_set_non_blocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1)
		return 1;
	flags |= O_NONBLOCK;

	return (fcntl(fd, F_SETFL, flags) == 0) ? 0 : 1;
}

/* Non-blocking server socket FD */
static int create_serv_sock(int *serv_fd, struct sockaddr_in *servaddr) 
{
	*serv_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (*serv_fd == -1)
		return 1;
	fd_set_non_blocking(*serv_fd);

	bzero(servaddr, sizeof(*servaddr));
	(*servaddr).sin_family = AF_INET;
	(*servaddr).sin_addr.s_addr = htonl(INADDR_ANY);
	(*servaddr).sin_port = htons(8080);
	if ((bind(*serv_fd, (struct sockaddr *)servaddr, sizeof(*servaddr))) != 0)
		return 1;

	return 0;
}

static void handle_conn(fd_buff_struct_t *fd_conn)
{
	fd_conn->state = STATE_END;
}

static void serv_accept_connection(int serv_fd, fd_buff_struct_t *net_fd_buffs, size_t net_fd_buffs_size) 
{
	int conn_fd = 0;

	do {
		struct sockaddr_in cli;
		int addrlen = sizeof(cli);
		conn_fd = accept(serv_fd, (struct sockaddr *)&cli, &addrlen);
		if (conn_fd < 0)
			break;
		fd_set_non_blocking(conn_fd);

		fprintf(stdout, "Accepted %s\n", inet_ntoa(cli.sin_addr));

		for (size_t i = 0; i < net_fd_buffs_size; ++i) {
			if (net_fd_buffs[i].fd <= 0) {
				fprintf(stdout, "Storing in net_fd_buffs[%d]\n", i);

				net_fd_buffs[i].fd = conn_fd;
				net_fd_buffs[i].state = STATE_REQ;
				net_fd_buffs[i].rbuff_size = 0;
				net_fd_buffs[i].wbuff_size = 0;
				net_fd_buffs[i].wbuff_sent = 0;
				break;
			}
		}
	} while (conn_fd != -1);
}

/* Event loop uses poll.h */
int main(void)
{
	int serv_fd;
	struct sockaddr_in servaddr;

	struct pollfd net_fds[NUM_CONNECTIONS - 1];
	size_t nfds = 1;
	fd_buff_struct_t *net_fd_buffs = malloc(sizeof(fd_buff_struct_t) * NUM_CONNECTIONS);

	if (create_serv_sock(&serv_fd, &servaddr) != 0)
		exit(1);
	if (listen(serv_fd, (int)(NUM_CONNECTIONS / 2)) < 0)
		exit(1);
	net_fds[0].fd = serv_fd;
	net_fds[0].events = POLLIN;

	/* Event loop */
	while (1) {
		nfds = 1;

		/* Prepare pollfd struct array */
		for (size_t i = 1; i < NUM_CONNECTIONS; ++i) {
			if (net_fd_buffs[i - 1].fd >= 0) {
				net_fds[i].fd = net_fd_buffs[i - 1].fd;
				net_fds[i].events = (net_fd_buffs[i - 1].state == STATE_REQ) ? POLLIN : POLLOUT;
				net_fds[i].events |= POLLERR;
				net_fds[i].revents = 0;

				nfds += 1;
			}
		}
		
		if (poll(net_fds, nfds, (5000)) > 0) {
			/* Check active connections */
			for (size_t i = 1; i < NUM_CONNECTIONS; ++i) {
				if (net_fds[i].fd > 0 && net_fds[i].revents) {
					handle_conn(&net_fd_buffs[i - 1]);

					/* Clean up array */
					if (net_fd_buffs[i - 1].fd > 0 && net_fd_buffs[i - 1].state == STATE_END) {
						fprintf(stdout, "Cleaning net_fd_buffs[%d] with FD %d\n", i - 1, net_fd_buffs[i - 1].fd);

						close(net_fd_buffs[i - 1].fd);

						net_fd_buffs[i - 1].fd = 0;
						net_fd_buffs[i - 1].state = STATE_REQ;
					}
				} 
			}

			/* Check listening socket for connections to accept */
			if (net_fds[0].revents & POLLIN) {
				serv_accept_connection(serv_fd, net_fd_buffs, NUM_CONNECTIONS);
			}
		}
	}

	/* Close FDs */
	for (size_t i = 0; i < LENGTH(net_fds); ++i) {
		if (net_fds[i].fd >= 0) 
			close(net_fds[i].fd);
	}

	for (size_t i = 0; i < NUM_CONNECTIONS; ++i) {
		if (net_fd_buffs[i].fd >= 0) 
			close(net_fd_buffs[i].fd);
	}
	free(net_fd_buffs);

	return 0;
}

