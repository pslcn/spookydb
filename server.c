#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h> 
#define MAX 80
#define PORT 8080
#define SA struct sockaddr

void handle_requests(int connfd)
{
}

int main(void)
{
	int sockfd, connfd, len;
	struct sockaddr_in servaddr, cli;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd == -1) exit(0);
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(PORT);
	if((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) exit(0);

	if((listen(sockfd, 1)) != 0) {
		printf("Listen failed");
		exit(0);
	}

	len = sizeof(cli);
	connfd = accept(sockfd, (SA*)&cli, &len);
	if(connfd < 0) exit(0);

	handle_requests(connfd);

	close(sockfd);
}

