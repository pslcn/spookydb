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

#include "non_blocking_server.h"
#include "http.h"

#define LENGTH(X) (sizeof X / sizeof X[0])

static void http_header_value_start(char *req, size_t req_size, size_t start_idx, size_t *header_value_idx)
{
    size_t after_colon = 0;

    for (size_t i = start_idx; i < req_size; ++i) {
        if (after_colon == 0) {
            if (req[i] == ':') {
                after_colon = 1;
            }
        } else {
            /* The HTTP/1.1 specification permits applications to use more spaces or no spaces between the colon and the header value */
            if (req[i] != ' ') {
                *header_value_idx = i;
                return;
            }
        }
    }
}

static void http_get_crlf_idx(char *req, size_t req_size, size_t start_idx, size_t *crlf_idx)
{
    for (size_t i = start_idx; i < req_size; ++i) {
        if (req[i] == '\r' && req[i + 1] == '\n') {
            *crlf_idx = i;
            return;
        }
    }
}

/* Returns idx of request body */
static size_t http_parse_headers(char *req, size_t req_size, size_t start_idx)
{
    size_t next_idx = start_idx;
    size_t header_value_idx, header_crlf_idx;

    while (1) {
        if (req[next_idx] == '\r' && req[next_idx + 1] == '\n') {
            return next_idx + 2;

        } else {
            http_get_crlf_idx(req, req_size, next_idx, &header_crlf_idx);

            if (header_crlf_idx - next_idx > 12 && strncmp(&req[next_idx], "Content-Type", 12) == 0) {
                http_header_value_start(req, req_size, next_idx, &header_value_idx);
                // char value[128];
                // memcpy(value, &req[header_value_idx], header_crlf_idx - header_value_idx);
                // value[header_crlf_idx - header_value_idx] = '\0';
                // fprintf(stdout, "[http_parse_headers] Value: %s\n", value);
            } 

            next_idx = header_crlf_idx + 2;
        }
    }
}

static void http_parse_message_line(char *req, size_t req_size, char *req_method, char *req_path, size_t *idx_headers)
{
    int spaces = 0;

    for (size_t c = 0; c < req_size; ++c) {
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

                http_get_crlf_idx(req, req_size, c, idx_headers);
                *idx_headers += 2;
                return;
            }
        }
    }
}

void http_parse_req(char *req, size_t req_size, char *req_method, char *req_path, char *req_body)
{
    size_t idx_headers, idx_req_body;

    http_parse_message_line(req, req_size, req_method, req_path, &idx_headers);
    idx_req_body = http_parse_headers(req, req_size, idx_headers);

    if (req[idx_req_body] == '\r' && req[idx_req_body + 1] == '\n') {
        memcpy(req_body, "", 1);
    } else {
        for (size_t c = idx_req_body; c < req_size; ++c) {
            req_body[c - idx_req_body] = req[c];
        }
        req_body[req_size - idx_req_body] = '\0';
    }
}

void http_handle_req(struct pollfd *conn_pollfd, struct fd_conn_buffs *fd_buffs)
{
    ssize_t rv;

    rv = recv(conn_pollfd->fd, &fd_buffs->rbuff.buff_content[fd_buffs->rbuff.buff_size], fd_buffs->rbuff.buff_capacity - fd_buffs->rbuff.buff_size, 0);

    if (rv > 0) {
        fd_buffs->rbuff.buff_size += rv;
    } else if (rv == 0) {
        fprintf(stdout, "[http_handle_req] Read %ld bytes\n", fd_buffs->rbuff.buff_size);
        fd_buffs->state = STATE_RES;
    } else if (rv == -1) {
        if (errno == EAGAIN && fd_buffs->rbuff.buff_size > 0) {
            fprintf(stdout, "[http_handle_req] Read %ld bytes\n", fd_buffs->rbuff.buff_size);
            fd_buffs->state = STATE_RES;
        } else {
            fprintf(stderr, "[http_handle_req] Error reading from FD: %s\n", strerror(errno));
        }
    }
}

void http_format_resp(char *resp, char *status, char *resp_headers, char *resp_body)
{
    size_t resp_num_chars = 9;
    memcpy(resp, "HTTP/1.1 ", resp_num_chars); /* Not null-terminated */

    memcpy(&resp[resp_num_chars], status, strlen(status));
    resp_num_chars += strlen(status);
    resp[resp_num_chars] = '\n';
    resp_num_chars += 1;

    memcpy(&resp[resp_num_chars], "Content-Length: ", 16); /* Not null-terminated */
    resp_num_chars += 16;

    /* Convert content length into string (ASCII) */
    int int_content_length = strlen(resp_body);
    char str_content_length[32];
    int curr;
    for (size_t i = 0; i < 32; ++i) {
        curr = int_content_length % 10;

        /* Getting ASCII code */
        str_content_length[i] = '0' + curr;

        int_content_length -= curr;
        int_content_length /= 10;
    }

    /* Get number of digits */
    size_t num_digits = 1;
    for (size_t i = 31; i > 0; --i) {
        if (str_content_length[i] != '0') {
            num_digits = i + 1;
            break;
        }
    }

    /* Reverse digits */
    char reversed_content_length[num_digits];
    for (size_t i = 0; i < num_digits; ++i) 
        reversed_content_length[i] = str_content_length[(num_digits - 1) - i]; 

    memcpy(&resp[resp_num_chars], reversed_content_length, num_digits);
    resp_num_chars += num_digits;
    memcpy(&resp[resp_num_chars], "\n\n", 2); /* Not null-terminated */
    resp_num_chars += 2;

    memcpy(&resp[resp_num_chars], resp_body, strlen(resp_body));
    resp_num_chars += strlen(resp_body);
    resp[resp_num_chars] = '\0';
}

static void http_send_wbuff(struct pollfd *conn_pollfd, struct fd_conn_buffs *fd_buffs)
{
    ssize_t rv;

    rv = send(conn_pollfd->fd, &fd_buffs->wbuff.buff_content[fd_buffs->wbuff_sent], fd_buffs->wbuff.buff_size - fd_buffs->wbuff_sent, 0);

    if (rv > 0) {
        fd_buffs->wbuff_sent += rv;
    } else {
        if (rv == -1) {
            fprintf(stderr, "[http_send_wbuff] Error sending to FD: %s\n", strerror(errno));
        }

        close(conn_pollfd->fd);
        conn_pollfd->fd = 0;
        fprintf(stdout, "[http_send_wbuff] Sent %ld bytes\n", fd_buffs->wbuff_sent);
        fd_buffs->state = STATE_READY;
    }
}

void http_handle_resp(struct pollfd *conn_pollfd, struct fd_conn_buffs *fd_buffs)
{
    if (fd_buffs->wbuff.buff_size == 0) {
        char req_method[7], req_path[BUFFSIZE], req_body[BUFFSIZE];

        http_parse_req(fd_buffs->rbuff.buff_content, fd_buffs->rbuff.buff_size, req_method, req_path, req_body);
        fprintf(stdout, "METHOD: %s PATH: %s BODY: %s\n", req_method, req_path, req_body); 

        if (strncmp(req_method, "GET", 4) == 0) {
            http_format_resp(fd_buffs->wbuff.buff_content, "200 OK", "Content-Type: text/plain", "NULL\n");
        } else if (strncmp(req_method, "PUT", 4) == 0) {
            http_format_resp(fd_buffs->wbuff.buff_content, "200 OK", "Content-Type: text/plain", "");
        } else if (strncmp(req_method, "DELETE", 7) == 0) {
            http_format_resp(fd_buffs->wbuff.buff_content, "200 OK", "Content-Type: text/plain", "");
        }

        fd_buffs->wbuff.buff_size = strlen(fd_buffs->wbuff.buff_content);
        fd_buffs->wbuff_sent = 0;
    } 

    http_send_wbuff(conn_pollfd, fd_buffs);
}

volatile sig_atomic_t stop;

void inthand(int signum)
{
    stop = 1;
}

static void save_connection(struct pollfd *pollfds, struct fd_conn_buffs *fd_buffs, int conn_fd, int *nfds)
{
    for (size_t i = 0; i < NUM_CONNECTIONS; ++i) {
        if (fd_buffs[i].state == STATE_READY) {
            fd_buffs[i].state = STATE_REQ;
            clear_rw_buff(&fd_buffs[i].rbuff);
            clear_rw_buff(&fd_buffs[i].wbuff);

            pollfds[i + 1].fd = conn_fd;
            pollfds[i + 1].events = POLLIN;
            pollfds[i + 1].events |= POLLERR;
            pollfds[i + 1].revents = 0;
            *nfds += 1;

            return;
        }
    }
}

static void check_listening_socket(struct pollfd *pollfds, struct fd_conn_buffs *fd_buffs, int *nfds)
{
    if (pollfds[0].revents & POLLIN) {
        int conn_fd;
        struct sockaddr_in cli;
        socklen_t addrlen = sizeof(cli);

        conn_fd = accept(pollfds[0].fd, (struct sockaddr *)&cli, &addrlen);
        fd_set_non_blocking(conn_fd);

        save_connection(pollfds, fd_buffs, conn_fd, nfds); 
    }
}

void handle_fd_io(struct pollfd *conn_pollfd, struct fd_conn_buffs *fd_buffs, int *nfds)
{
    switch (fd_buffs->state) {
        case STATE_REQ:
            http_handle_req(conn_pollfd, fd_buffs);
            break;
        case STATE_RES:
            http_handle_resp(conn_pollfd, fd_buffs);
            if (fd_buffs->state == STATE_READY)
                *nfds -= 1;
            break;
    }
}

static void reset_pollfd_events(struct pollfd *conn_pollfd, struct fd_conn_buffs *fd_buffs)
{
    conn_pollfd->events = (fd_buffs->state == STATE_REQ) ? POLLIN : POLLOUT;
    conn_pollfd->events |= POLLERR;
    conn_pollfd->revents = 0;
}

void http_serve(struct pollfd *pollfds, struct fd_conn_buffs *fd_buffs)
{
    signal(SIGINT, inthand);

    int nfds = 1;

    while (!stop) {
        for (size_t i = 1; i < NUM_CONNECTIONS + 1; ++i) {
            handle_fd_io(&pollfds[i], &fd_buffs[i - 1], &nfds);
            reset_pollfd_events(&pollfds[i], &fd_buffs[i - 1]);
        }

        poll(pollfds, nfds, 10); 
        if (nfds < NUM_CONNECTIONS + 1)
            check_listening_socket(pollfds, fd_buffs, &nfds);
    }
}

#if 0
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

    return 0;
}
#endif 

