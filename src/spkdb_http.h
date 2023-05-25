#ifndef HTTP_H_
#define HTTP_H_

#include "nb_fd_io.h"

#define BUFFSIZE 1024
#define NUM_CONNECTIONS 32

#define RESP_LEN 1024

typedef struct {
	/* Only supports GET, PUT, and DELETE; DELETE is 6 characters */
	char req_method[8];
	char req_path[BUFFSIZE];
	char req_body[BUFFSIZE];
} parsed_http_req_t;

void http_handle_res(fd_buff_struct_t *fd_conn);
void http_parse_req(char *req, char *req_method, char *req_path, char *req_body, size_t content_buff_size);
void http_write_resp(char *resp, char *status, char *resp_headers, char *resp_body);

#endif
