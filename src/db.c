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

int fd_set_non_blocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1) 
		return 1;
	flags |= O_NONBLOCK;

	return (fcntl(fd, F_SETFL, flags) == 0) ? 0 : 1;
}

/* Non-blocking server socket FD */
int create_serv_sock(int *serv_fd, struct sockaddr_in *servaddr) 
{
	*serv_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (*serv_fd == -1)
		exit(1);
	fd_set_non_blocking(*serv_fd);

	bzero(servaddr, sizeof(*servaddr));
	(*servaddr).sin_family = AF_INET;
	(*servaddr).sin_addr.s_addr = htonl(INADDR_ANY);
	(*servaddr).sin_port = htons(8080);
	if ((bind(*serv_fd, (struct sockaddr *)servaddr, sizeof(*servaddr))) != 0)
		exit(1);

	return 0;
}

/* Event loop uses poll.h */
int main(void) 
{
	int serv_fd;
	struct sockaddr_in servaddr;
	struct pollfd net_fds[32] = { 0 };
	int nfds = 1;
	int conn_fd = 0;

	create_serv_sock(&serv_fd, &servaddr);
	if (listen(serv_fd, 32) < 0)
		exit(1);

	net_fds[0].fd = serv_fd;
	net_fds[0].events = POLLIN;

	while (1) {
		if (poll(net_fds, nfds, (5000)) > 0) {
			/* Accept connection */
			if (net_fds[0].revents & POLLIN) { 
				struct sockaddr_in cli;
				int addrlen = sizeof(cli);
				conn_fd = accept(serv_fd, (struct sockaddr *)&cli, &addrlen);
				if (conn_fd < 0)
					break;

				fprintf(stdout, "Accepted %s\n", inet_ntoa(cli.sin_addr));
				for (size_t i = 1; i < 32; ++i) {
					if (net_fds[i].fd <= 0) {
						net_fds[i].fd = conn_fd;
						net_fds[i].events = POLLIN;
						nfds += 1;
						break;
					}
				}

				/*
				while (conn_fd != -1) {
					struct sockaddr_in cli;
					int addrlen = sizeof(cli);
					conn_fd = accept(serv_fd, (struct sockaddr *)&cli, &addrlen);
					if (conn_fd < 0) 
						break;

					printf("Accepted %s\n", inet_ntoa(cli.sin_addr));
					for (size_t i = 1; i < 32; ++i) {
						if (net_fds[i].fd <= 0) {
							net_fds[i].fd = conn_fd;
							net_fds[i].events = POLLIN;
							nfds += 1;
							break;
						}
					}
				}
				*/
			}

			/* Check existing connections */
			for (size_t i = 1; i < 32; ++i) {
				if (net_fds[i].fd > 0 && net_fds[i].revents & POLLIN) {
					printf("Closing net_fds[%d].fd (%d)\n", i, net_fds[i].fd);
					close(net_fds[i].fd);
					net_fds[i].fd = -1;
				}
			}

			/* Clean up array */
			for (size_t i = 1; i < 32; ++i) {
				if (net_fds[i].fd < 0) {
					net_fds[i].fd = 0;
					net_fds[i].events = 0;
					net_fds[i].revents = 0;
					nfds -= 1;
				}
			}
		}
	}

	/* Close FDs */
	for (size_t i = 0; i < 32; ++i) {
		if (net_fds[i].fd >= 0)
			close(net_fds[i].fd);
	}

	return 0;
}

