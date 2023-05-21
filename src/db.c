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

#define RESP_LEN 1024

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

	/* Only supports GET, PUT, and DELETE; DELETE is 6 characters */
	char req_method[8];
	char req_path[BUFFSIZE];
	char req_body[BUFFSIZE];

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

static void handle_res(fd_buff_struct_t *fd_conn)
{
	fprintf(stdout, "Sending response of %d bytes to FD %d\n", fd_conn->wbuff_size, fd_conn->fd);
	ssize_t rv = write(fd_conn->fd, &fd_conn->wbuff, fd_conn->wbuff_size);

	fd_conn->state = STATE_END;
}

static void parse_req(char *req, char *req_method, char *req_path, char *req_body, size_t content_buff_size)
{
	/* Number of spaces; whether string is request body */
	int spaces = 0, isbody = 0;

	for (size_t c = 0; c < content_buff_size; ++c) {
		if (spaces == 0) {
			if (req[c] != ' ') 
				req_method[c] = req[c];	
			else {
				req_method[c] = '\0';
				spaces = c + 1;
			}
		} else {
			if (req[c] != ' ') 
				req_path[c - spaces] = req[c];
			else {
				req_path[c - spaces] = '\0';
				break;
			}
		}
	}

	/* Parse request body */
	if (strncmp(req_method, "PUT", 4) == 0) {
		for (size_t c = 0; c < content_buff_size; ++c) {
			if (isbody == 0) {
				if ((req[c] == '\n') && (req[c + 2] == '\n')) {
					c += 2;
					isbody = c + 1;
				}
			} else {
				req_body[c - isbody] = req[c];
			}
		}
	} else 
		strncpy(req_body, "NULL", 5);	
}

/* Simple network responses */
static void write_resp(char *resp, char *status, char *resp_headers, char *resp_body)
{
	size_t resp_num_chars = 9;

	strncpy(resp, "HTTP/1.1 ", resp_num_chars); /* Should not be null-terminated */

	strncpy(&resp[resp_num_chars], status, strlen(status));
	resp_num_chars += strlen(status);
	resp[resp_num_chars] = '\n';
	resp_num_chars += 1;
	
	strncpy(&resp[resp_num_chars], resp_headers, strlen(resp_headers));
	resp_num_chars += strlen(resp_headers);
	resp[resp_num_chars] = '\n';
	resp_num_chars += 1;

	strncpy(&resp[resp_num_chars], "Content-Length: ", 16); /* Should not be null-terminated */
	resp_num_chars += 16;

	/* Convert content length into string (ASCII) */
	int int_content_length = strlen(resp_body);
	char str_content_length[32];
	for (size_t i = 0; i < 32; ++i) {
		int curr = int_content_length % 10;

		/* Getting ASCII code */
		str_content_length[i] = '0' + curr;

		int_content_length -= curr;
		int_content_length /= 10;
	}

	/* Get number of digits */
	int num_digits = 1;
	for (size_t i = 31; i > 0; --i) {
		if (str_content_length[i] != '0') {
			num_digits = i + 1;
			break;
		}
	}

	/* Reverse digits */
	char reversed_content_length[num_digits];
	for (size_t i = 0; i < num_digits; ++i) {
		reversed_content_length[i] = str_content_length[(num_digits - 1) - i];
	}

	strncpy(&resp[resp_num_chars], &reversed_content_length, num_digits);
	resp_num_chars += num_digits;
	strncpy(&resp[resp_num_chars], "\n\n", 2);
	resp_num_chars += 2;

	strncpy(&resp[resp_num_chars], resp_body, strlen(resp_body));
	resp_num_chars += strlen(resp_body);
	resp[resp_num_chars] = '\0';
}

static void handle_req(fd_buff_struct_t *fd_conn)
{
	ssize_t rv = 0;

	do {
		rv = read(fd_conn->fd, &fd_conn->rbuff[fd_conn->rbuff_size], sizeof(fd_conn->rbuff) - fd_conn->rbuff_size);
	} while (rv < 0 && errno == EINTR);

	fd_conn->rbuff_size += (size_t)rv;

	parse_req(&fd_conn->rbuff, &fd_conn->req_method, &fd_conn->req_path, &fd_conn->req_body, fd_conn->rbuff_size);
	fprintf(stdout, "METHOD: %s PATH: %s BODY: %s\n", fd_conn->req_method, fd_conn->req_path, &fd_conn->req_body);

	/* Handle request method */
	char resp[RESP_LEN];

	if (strncmp(fd_conn->req_method, "GET", 4) == 0) {
		write_resp(&resp, "200 OK", "Content-Type: text/plain", "GET!\n");
	} else if (strncmp(fd_conn->req_method, "PUT", 4) == 0) {
		write_resp(&resp, "200 OK", "Content-Type: text/plain", "PUT!\n");
	} else if (strncmp(fd_conn->req_method, "DELETE", 7) == 0) {
		write_resp(&resp, "200 OK", "Content-Type: text/plain", "DELETE!\n");
	}

	size_t resp_bytes = 0;
	for (size_t i = 0; i < LENGTH(resp); ++i) {
		if (resp[i] == '\0') {
			resp_bytes = i;
			break;
		}	
	}

	memcpy(&fd_conn->wbuff, &resp, resp_bytes);
	fd_conn->wbuff_size = resp_bytes;

	fd_conn->state = STATE_RES;
	handle_res(fd_conn);
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

	struct pollfd net_fds[NUM_CONNECTIONS];
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
					/* Handle connection */
					if (net_fd_buffs[i - 1].state == STATE_REQ)
						handle_req(&net_fd_buffs[i - 1]);
					else if (net_fd_buffs[i - 1].state == STATE_RES)
						handle_res(&net_fd_buffs[i - 1]);

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
	fprintf(stdout, "Closing listening socket FD %d\n", serv_fd);
	close(serv_fd);
	for (size_t i = 0; i < NUM_CONNECTIONS; ++i) {
		if (net_fd_buffs[i].fd > 0) {
			fprintf(stdout, "Closing FD %d in net_fd_buffs[%d]\n", net_fd_buffs[i].fd, i);

			close(net_fd_buffs[i].fd);
		}
	}

	free(net_fd_buffs);

	return 0;
}

