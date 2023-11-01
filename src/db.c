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

#include "hashtable.h"
#include "non_blocking_server.h"
#include "http.h"

static struct htable *ht;

#if 0
void http_handle_req(struct fd_buff_handler *fd_conn, parsed_http_req_t *parsed_http_req)
{
        /* (Indexing req_path at 1 excludes '/') */
        if (strncmp(parsed_http_req->req_method, "GET", 4) == 0) {
                char *value_pointer = htable_search(ht, &parsed_http_req->req_path[1]);

                if (value_pointer != NULL) {
                        http_write_resp(resp, "200 OK", "Content-Type: text/plain", value_pointer);
                } else {
                        http_write_resp(resp, "200 OK", "Content-Type: text/plain", "NULL\n");
                }

        } else if (strncmp(parsed_http_req->req_method, "PUT", 4) == 0) {
                htable_insert(ht, &parsed_http_req->req_path[1], parsed_http_req->req_body);
                http_write_resp(resp, "200 OK", "Content-Type: text/plain", "");

        } else if (strncmp(parsed_http_req->req_method, "DELETE", 7) == 0) {
                htable_remove(ht, &parsed_http_req->req_path[1]); 
                http_write_resp(resp, "200 OK", "Content-Type: text/plain", "");
        }
}
#endif

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

        create_htable(&ht);
        htable_insert(ht, "Hello", "World");

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

