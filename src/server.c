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

#include "server.h"

#define PORT 8080
#define SA struct sockaddr
#define BUFFER_SIZE 1024

typedef struct TestStore {
	char *key;
	char *value;
} dict_t;

int sockfd;

void write_response(int connfd, char status[], char response_headers[], char response_body[])
{
	char *response = ("HTTP/1.1 %s\n%s\nContent-Length: %d\n\n%s", status, response_headers, strlen(response_body), response_body); 
	write(connfd, response, strlen(response));
}

void handle_requests(int connfd)
{
	if(connfd < 0) exit(1);

	char req[BUFFER_SIZE];
	bzero(req, BUFFER_SIZE);
	read(connfd, req, BUFFER_SIZE - 1);

	char *req_method, *req_path, *req_query;
	req_method = strtok(req, " ");
	req_path = strtok(strtok(NULL, " "), "?");
	req_query = strtok(NULL, "?");

	dict_t store_to_test;  
	store_to_test.key = "index";
	store_to_test.value = "You have accessed the key-value store!";
		
	char *response_body;
	if(strcmp(req_method, "GET")) {
		response_body = store_to_test.value;
	} else if(strcmp(req_method, "PUT")) {
		response_body = store_to_test.value;
	}

	write_response(connfd, "200 OK", "Content-Type: text/html", response_body);

	close(connfd);
}

void start_server(void)
{
	int len;
	struct sockaddr_in servaddr, cli;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd == -1) exit(1);
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(PORT);
	if((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) exit(1);

	if(listen(sockfd, 4) < 0) exit(1);

	len = sizeof(cli);
	handle_requests(accept(sockfd, (SA*)&cli, &len));
}

void stop_server(void)
{
	close(sockfd);
}

