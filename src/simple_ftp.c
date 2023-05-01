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

#define BUFFSIZE 1024

typedef struct {
	int f_fd; /* Open file */
	char chunk_buff[BUFFSIZE];
	int chunk_buff_len;
	int chunk_buff_used;
	int is_sending;
} file_conn_t; /* Perhaps separate integer for status is superfluous */

int fd_set_nb(int fd) /* Set FD to non-blocking */
{
	int flags;
	if ((flags = fcntl(fd, F_GETFL, 0)) == -1)
		flags = 0;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main(void) 
{
	int serv_sock_fd;
	struct sockaddr_in servaddr, cli;
	int addr_len = sizeof(cli);
	int conn_fd;
	struct pollfd net_conn_fds[4];

	/* Bind server socket */
	if ((serv_sock_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		exit(1);
	fd_set_nb(serv_sock_fd);
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(8080);
	if ((bind(serv_sock_fd, (struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) 
		exit(1);

	if (listen(serv_sock_fd, 4) < 0) 
		exit(1);

	/* Assign listening FD */
	net_conn_fds[0] = (struct pollfd){serv_sock_fd, POLLIN | POLLPRI, 0};

	/* Non-blocking event loop */
	int num_active = 0;
	while (1) {
		if ((poll(net_conn_fds, num_active + 1, 1000)) > 0) {
			if (net_conn_fds[0].revents & POLLIN) { /* Accept connection */
				/* addr_len = sizeof(cli); */
				conn_fd = accept(serv_sock_fd, (struct sockaddr *)&cli, &addr_len);
				printf("New connection: %d\n", conn_fd);
				for (size_t i = 1; i < 5; ++i) {
					printf("net_conn_fds[%d]: %d\n", i, net_conn_fds[i].fd);
					if (net_conn_fds[i].fd == 0) {
						net_conn_fds[i] = (struct pollfd){conn_fd, POLLIN | POLLPRI, 0};
						++num_active;
						printf("conn_fd stored at index %d of net_conn_fds\n", i);
						break;
					}
				}
			}

			for (size_t i = 1; i < 5; ++i) {
				printf("net_conn_fds[%d]: %d\n", i, net_conn_fds[i].fd);
				if (net_conn_fds[i].fd > 0 && net_conn_fds[i].revents & POLLIN) {
					printf("%d has input\n", net_conn_fds[i].fd);
					char buff[BUFFSIZE];
					int buff_size = read(net_conn_fds[i].fd, buff, BUFFSIZE - 1);
					if (buff_size <= 0) {
						net_conn_fds[i] = (struct pollfd){0, 0, 0};
						--num_active;
					} else {
						buff[buff_size] = '\0';
						printf("%s\n", buff);
					}
				}
			}
		}
	}

	close(conn_fd);
	close(serv_sock_fd);
	return 0;
}

