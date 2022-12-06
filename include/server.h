#ifndef SERVER_H_
#define SERVER_H_

typedef struct TestStore dict_t;

void write_response(int connfd, char status[], char response_headers[], char response_body[]);
void handle_requests(int connfd);
void start_server(void);
void stop_server(void);

#endif
