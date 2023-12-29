#ifndef SPKDB_HTTP_H_
#define SPKDB_HTTP_H_


#define BUFFSIZE 1024
#define NUM_CONNECTIONS 32
#define RESP_LEN 1024


void http_parse_req(char *req, size_t req_size, char *req_method, char *req_path, char *req_body);
void http_format_res(char *resp, char *status, char *resp_headers, char *resp_body);
void http_handle_req(struct pollfd *conn_pollfd, struct fd_conn_buffs *fd_buffs);
void http_handle_res(struct pollfd *conn_pollfd, struct fd_conn_buffs *fd_buffs);
void http_serve(struct pollfd *pollfds, struct fd_conn_buffs *fd_buffs);

#endif

