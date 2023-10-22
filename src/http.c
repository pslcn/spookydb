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

#include "non_blocking_fd_io.h"

#define LENGTH(X) (sizeof X / sizeof X[0])

#define BUFFSIZE 1024
#define NUM_CONNECTIONS 32

#define RESP_LEN 1024

struct parsed_http_req {
  char *req_method, *req_path, *req_body;
};

/* Only supports GET, PUT, and DELETE; DELETE is 6 characters */
int create_parsed_http_req(struct parsed_http_req *parsed_http_req)
{
  parsed_http_req->req_method = malloc(sizeof(char) * 6);
  parsed_http_req->req_path = malloc(sizeof(char) * BUFFSIZE);
  parsed_http_req->req_body = malloc(sizeof(char) * BUFFSIZE);
  return 0;
}


void http_parse_req(char *req, char *req_method, char *req_path, char *req_body, size_t content_buff_size)
{
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
    req_body[(content_buff_size - isbody) + 1] = '\0';
  } else 
    strncpy(req_body, "NULL", 5); 
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


void http_handle_req(struct fd_buff_handler *fd_conn, struct parsed_http_req *parsed_http_req)
{
  /* Read into fd_conn->rbuff.buff_content and parse the HTTP request */
  fd_buff_buffered_read(fd_conn);

  http_parse_req(fd_conn->rbuff.buff_content, parsed_http_req->req_method, parsed_http_req->req_path, parsed_http_req->req_body, fd_conn->rbuff.buff_size);
  fprintf(stdout, "METHOD: %s PATH: %s BODY: %s\n", parsed_http_req->req_method, parsed_http_req->req_path, parsed_http_req->req_body); 

  char resp[RESP_LEN];
  /* (Indexing req_path at 1 excludes '/') */
  if (strncmp(parsed_http_req->req_method, "GET", 4) == 0) {
    http_format_resp(resp, "200 OK", "Content-Type: text/plain", "NULL\n");

  } else if (strncmp(parsed_http_req->req_method, "PUT", 4) == 0) {
    http_format_resp(resp, "200 OK", "Content-Type: text/plain", "");

  } else if (strncmp(parsed_http_req->req_method, "DELETE", 7) == 0) {
    http_format_resp(resp, "200 OK", "Content-Type: text/plain", "");
  }

  size_t resp_bytes = 0;
  for (size_t i = 0; i < LENGTH(resp); ++i) {
    if (resp[i] == '\0') {
      resp_bytes = i;
      break;
    } 
  }

  /* Copy response to fd_conn->wbuff.buff_content and switch to response state */
  memcpy(fd_conn->wbuff.buff_content, &resp, resp_bytes);
  fd_conn->wbuff.buff_size = resp_bytes;
  fd_conn->state = STATE_RES;
  fd_buff_write_content(fd_conn);
}


int main(void) 
{
  int serv_fd;
  struct sockaddr_in servaddr;

  struct pollfd serv_pollfd_array[NUM_CONNECTIONS];
  struct fd_buff_handler serv_pollfd_buffs[NUM_CONNECTIONS];
  struct parsed_http_req parsed_http_reqs[NUM_CONNECTIONS];
  size_t nfds = 1;

  for (size_t i = 0; i < NUM_CONNECTIONS; ++i) {
    create_fd_buff_handler(&(serv_pollfd_buffs[i]), BUFFSIZE, BUFFSIZE);
    create_parsed_http_req(&(parsed_http_reqs[i]));
  }

  if (create_serv_sock(&serv_fd, &servaddr, 8080) != 0)
    exit(1);
  if (listen(serv_fd, (int)(NUM_CONNECTIONS / 2)) < 0)
    exit(1);
  serv_pollfd_array[0].fd = serv_fd;
  serv_pollfd_array[0].events = POLLIN;

  while(1) {
    /* Blocks until pollfd array has been prepared */
    if (prepare_pollfd_array(serv_pollfd_buffs, &(serv_pollfd_array[1]), NUM_CONNECTIONS, &nfds) == 0) {
      if (poll(serv_pollfd_array, nfds, 5000) > 0) {
        for (size_t i = 1; i < NUM_CONNECTIONS; ++i) {
          if (serv_pollfd_array[i].fd > 0 && serv_pollfd_array[i].revents) {
            if (serv_pollfd_buffs[i - 1].state == STATE_REQ) {
              http_handle_req(&(serv_pollfd_buffs[i - 1]), &parsed_http_reqs[i - 1]);
            } else if (serv_pollfd_buffs[i - 1].state == STATE_RES)
              fd_buff_write_content(&(serv_pollfd_buffs[i - 1]));

            /* Clean up array */
            if (serv_pollfd_buffs[i - 1].fd > 0 && serv_pollfd_buffs[i - 1].state == STATE_END) {
              fprintf(stdout, "%p: Closing FD %d in serv_pollfd_buffs[%ld]\n", serv_pollfd_buffs + (i - 1), serv_pollfd_buffs[i - 1].fd, i - 1);

              close(serv_pollfd_buffs[i - 1].fd);
              serv_pollfd_buffs[i - 1].fd = 0;
            }
          } 
        }

        /* Check listening socket for connections to accept */
        if (serv_pollfd_array[0].revents & POLLIN) {
          serv_accept_connection(serv_fd, serv_pollfd_buffs, NUM_CONNECTIONS);
        }
      }
    }
  }

  /* Close FDs */
  fprintf(stdout, "Closing listening socket FD %d\n", serv_fd);
  close(serv_fd);
  for (size_t i = 0; i < NUM_CONNECTIONS; ++i) {
    if (serv_pollfd_buffs[i].fd > 0) {
      fprintf(stdout, "Closing FD %d in serv_pollfd_buffs[%ld]\n", serv_pollfd_buffs[i].fd, i);
      close(serv_pollfd_buffs[i].fd);
    }
  }
  return 0;
}

