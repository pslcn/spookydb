#ifndef HTTP_H_
#define HTTP_H_

#define BUFFSIZE 1024
#define NUM_CONNECTIONS 32
#define RESP_LEN 1024

struct parsed_http_req {
  char *req_method, *req_path, *req_body;
};

int create_parsed_http_req(struct parsed_http_req *parsed_http_req);
void clear_parsed_http_req(struct parsed_http_req *parsed_http_req);
void http_parse_req(char *req, struct parsed_http_req *parsed_http_req, size_t content_buff_size);
void http_format_resp(char *resp, char *status, char *resp_headers, char *resp_body);
void http_handle_req(struct fd_buff_handler *fd_conn, struct parsed_http_req *parsed_http_req);

#endif

