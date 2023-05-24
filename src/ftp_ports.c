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

#define LENGTH(X) (sizeof X / sizeof X[0])

#define BUFFSIZE 1024
#define NUM_CONNECTIONS 32

typedef struct {
	int fd; 

	size_t rbuff_size;
	uint8_t rbuff[BUFFSIZE];
} port_buff_struct_t;

/* Active FTP; PASV is not currently supported */
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

/* Concurrency management with poll.h */
int ftp_poll_ports(ftp_ports_t *ftp_handler)
{
	size_t nfds = 1;

	return 0;
}

static int fd_set_non_blocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1)
		return 1;
	flags |= O_NONBLOCK;

	return (fcntl(fd, F_SETFL, flags) == 0) ? 0 : 1;
}

static int create_sock(int *sock_fd, struct sockaddr_in *servaddr, int port)
{
	*sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (*sock_fd == -1)
		return 1;
	fd_set_non_blocking(*sock_fd);

	fprintf(stdout, "(create_sock) Created non-blocking socket at %p\n", sock_fd);

	bzero(servaddr, sizeof(*servaddr));
	servaddr->sin_family = AF_INET;
	servaddr->sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr->sin_port = htons(port);

	fprintf(stdout, "(create_sock) Assigning socket address at %p to socket\n", servaddr);
	if ((bind(*sock_fd, (struct sockaddr *)servaddr, sizeof(*servaddr))) != 0) {
		if (errno == EADDRINUSE)
			fprintf(stdout, "Address already in use\n");
		
		/* Running as root fixes */
		else if (errno == EACCES)
			fprintf(stdout, "Error\n");

		return 1;
	}

	return 0;
}

/* Concurrency management with poll.h */
int create_ftp_handler(ftp_ports_t *ftp_handler)
{
	ftp_handler = malloc(sizeof(ftp_ports_t));
	fprintf(stdout, "(create_ftp_handler) ftp_handler created at %p\n", ftp_handler);

	/* Communication on port 21; data transfer on port 20 */
	fprintf(stdout, "(create_ftp_handler) Creating port 20 socket at %p\n", &(ftp_handler->serv_fd_20));
	if (create_sock(&(ftp_handler->serv_fd_20), &(ftp_handler->servaddr_20), 20) != 0)
		return 1;

	fprintf(stdout, "(create_ftp_handler) Creating port 21 socket at %p\n", &(ftp_handler->serv_fd_21));
	if (create_sock(&(ftp_handler->serv_fd_21), &(ftp_handler->servaddr_21), 21) != 0)
		return 1;

	if (listen(ftp_handler->serv_fd_20, (int)(NUM_CONNECTIONS / 2)) < 0)
		return 1;
	if (listen(ftp_handler->serv_fd_21, (int)(NUM_CONNECTIONS / 2)) < 0)
		return 1;

	ftp_handler->port_21_fds = calloc(NUM_CONNECTIONS, sizeof(struct pollfd *));
	ftp_handler->port_20_fds = calloc(NUM_CONNECTIONS, sizeof(struct pollfd *));
	for (size_t i = 0; i < NUM_CONNECTIONS; ++i) {
		ftp_handler->port_21_fds[i] = malloc(sizeof(struct pollfd));
		ftp_handler->port_20_fds[i] = malloc(sizeof(struct pollfd));
	}

	ftp_handler->port_21_fds_size = NUM_CONNECTIONS;
	ftp_handler->port_20_fds_size = NUM_CONNECTIONS;

	ftp_handler->port_21_fds[0]->fd = ftp_handler->serv_fd_21;
	ftp_handler->port_21_fds[0]->events = POLLIN;

	ftp_handler->port_20_buffs = malloc(sizeof(port_buff_struct_t) * NUM_CONNECTIONS);
	ftp_handler->port_21_buffs = malloc(sizeof(port_buff_struct_t) * NUM_CONNECTIONS);

	return 0;
}

int close_ftp_handler(ftp_ports_t *ftp_handler)
{
	if (ftp_handler == NULL)
		return 1;

	fprintf(stdout, "Closing ftp_handler at %p\n", ftp_handler);

	if (ftp_handler->serv_fd_21 > 0) 
		close(ftp_handler->serv_fd_21);

	if (ftp_handler->port_21_fds != NULL) {
		fprintf(stdout, "(close_ftp_handler) port_21_fds are at %p\n", ftp_handler->port_21_fds);

		for (size_t i = 0; i < NUM_CONNECTIONS; ++i) {
			if (ftp_handler->port_21_fds[i] == NULL)
				continue;

			fprintf(stdout, "ftp_handler->port_21_fds[%d]: %p\n", i, ftp_handler->port_21_fds[i]);

			if (ftp_handler->port_21_fds[i]->fd > 0) 
				close(ftp_handler->port_21_fds[i]->fd);

			fprintf(stdout, "Freeing %p\n", ftp_handler->port_21_fds[i]);
			free(ftp_handler->port_21_fds[i]);	
		}

		free(ftp_handler->port_21_fds);
	}

	if (ftp_handler->port_20_fds != NULL) {
		fprintf(stdout, "(close_ftp_handler) port_20_fds are at %p\n", ftp_handler->port_20_fds);

		for (size_t i = 0; i < NUM_CONNECTIONS; ++i) {
			if (ftp_handler->port_20_fds[i] != NULL)
				free(ftp_handler->port_20_fds[i]);
		}

		free(ftp_handler->port_20_fds);
	}

	if (ftp_handler->port_21_buffs != NULL)
		free(ftp_handler->port_21_buffs);
	if (ftp_handler->port_20_buffs != NULL)
		free(ftp_handler->port_20_buffs);

	return 0;
}

/* Testing */
#if 1
int main(void)
{
	ftp_ports_t ftp_handler;

	if (create_ftp_handler(&ftp_handler) != 0)
		exit(1);

	while (1) {
		fprintf(stdout, "ftp_poll_ports: %d\n", ftp_poll_ports(&ftp_handler));
		break;
	}

	close_ftp_handler(&ftp_handler);

	return 0;
}
#endif

