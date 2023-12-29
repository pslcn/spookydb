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

#include "../src/non_blocking_server.h"
#include "../src/spkdb_http.h"

int main(void) 
{
        int serv_fd;
        struct sockaddr_in servaddr;
        struct pollfd pollfds[NUM_CONNECTIONS + 1];
        struct fd_conn_buffs fd_buffs[NUM_CONNECTIONS];

        for (size_t i = 0; i < NUM_CONNECTIONS; ++i) {
                pollfds[i].fd = 0;
                create_fd_conn_buffs(&fd_buffs[i], BUFFSIZE);
        }

        if (create_serv_sock(&serv_fd, &servaddr, 8080) != 0)
                exit(1);
        if (listen(serv_fd, (int)(NUM_CONNECTIONS / 2)) < 0)
                exit(1);
        pollfds[0].fd = serv_fd;
        pollfds[0].events = POLLIN;

        http_serve(pollfds, fd_buffs);

        printf("Exiting\n");

        return 0;
}
