#ifndef HTTP_H_
#define HTTP_H_

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

#define LENGTH(X) (sizeof X / sizeof X[0])

#define BUFFSIZE 1024
#define NUM_CONNECTIONS 32

#define RESP_LEN 1024

typedef struct {
	/* Only supports GET, PUT, and DELETE; DELETE is 6 characters */
	char *req_method, *req_path, *req_body;
} parsed_http_req_t;

void http_handle_res(fd_buff_struct_t *fd_conn);
void http_parse_req(char *req, char *req_method, char *req_path, char *req_body, char content_buff_size);
void http_write_resp(char *resp, char *status, char *resp_headers, char *resp_body);

int create_parsed_http_req(parsed_http_req_t **parsed_req);
int create_parsed_http_req_array(parsed_http_req_t ***parsed_http_reqs, size_t parsed_http_reqs_size);

void http_handle_res(fd_buff_struct_t *fd_conn)
{
	fprintf(stdout, "Sending response of %d bytes to FD %d\n", fd_conn->wbuff_size, fd_conn->fd);
	ssize_t rv = write(fd_conn->fd, &fd_conn->wbuff, fd_conn->wbuff_size);

	fd_conn->state = STATE_END;
}

void http_parse_req(char *req, char *req_method, char *req_path, char *req_body, size_t content_buff_size)
{
	fprintf(stdout, "content_buff_size: %d\n", content_buff_size);

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

	fprintf(stdout, "path: %s\n", req_path);

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
void http_write_resp(char *resp, char *status, char *resp_headers, char *resp_body)
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

int create_parsed_http_req(parsed_http_req_t **parsed_req)
{
	*parsed_req = malloc(sizeof(parsed_http_req_t));

	(*parsed_req)->req_method = calloc(8, sizeof(char *));
	(*parsed_req)->req_path = calloc(BUFFSIZE, sizeof(char *));
	(*parsed_req)->req_body = calloc(BUFFSIZE, sizeof(char *));

	for (size_t i = 0; i < 8; ++i) 
		(*parsed_req)->req_method[i] = malloc(sizeof(char));
	
	for (size_t i = 0; i < BUFFSIZE; ++i) {
		(*parsed_req)->req_path[i] = malloc(sizeof(char));
		(*parsed_req)->req_body[i] = malloc(sizeof(char));
	}

	return 0;
}

int create_parsed_http_req_array(parsed_http_req_t ***parsed_http_reqs, size_t parsed_http_reqs_size)
{
	*parsed_http_reqs = calloc(parsed_http_reqs_size, sizeof(parsed_http_req_t *));
	fprintf(stdout, "%p: created parse_http_req_t array with size %d\n", *parsed_http_reqs, parsed_http_reqs_size);

	for (size_t i = 0; i < parsed_http_reqs_size; ++i) {
		create_parsed_http_req(&(*parsed_http_reqs)[i]);	
	}

	return 0;
}

#endif
