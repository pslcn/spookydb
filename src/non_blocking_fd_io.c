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

int create_buff(buff_t *buff, size_t buff_capacity)
{
  buff->buff_capacity = buff_capacity;
  buff->buff_content = calloc(buff_capacity, sizeof(uint8_t));
  return 0;
}

int create_fd_buff_struct(fd_buff_struct_t *fd_buff, size_t rbuff_capacity, size_t wbuff_capacity)
{
  fd_buff->fd = 0;
  create_buff(&(fd_buff->rbuff), rbuff_capacity); 
  create_buff(&(fd_buff->wbuff), wbuff_capacity); 
  return 0;
}

int fd_set_non_blocking(int fd)
{
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1)
    return 1;
  flags |= O_NONBLOCK;
  return (fcntl(fd, F_SETFL, flags) == 0) ? 0 : 1;
}

int create_serv_sock(int *serv_fd, struct sockaddr_in *servaddr, int port)
{
  *serv_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (*serv_fd == -1)
    return 1;
  fd_set_non_blocking(*serv_fd);

  int enable = 1;
  if (setsockopt(*serv_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    return 1;

  bzero(servaddr, sizeof(*servaddr));
  (*servaddr).sin_family = AF_INET;
  (*servaddr).sin_addr.s_addr = htonl(INADDR_ANY);
  (*servaddr).sin_port = htons(port);
  if ((bind(*serv_fd, (struct sockaddr *)servaddr, sizeof(*servaddr))) != 0) {
    if (errno == EADDRINUSE)
      fprintf(stdout, "Address already in use\n");
    else if (errno == EACCES)
      fprintf(stdout, "EACCES error\n");
    return 1;
  }
  return 0;
}

int prepare_pollfd_array(fd_buff_struct_t *fd_buff_structs, struct pollfd *pollfd_array, size_t pollfd_array_size, size_t *nfds)
{
  *nfds = 1;
  for (size_t i = 1; i < pollfd_array_size; ++i) {
    if (fd_buff_structs[i - 1].fd > 0) {
      pollfd_array[i].fd = fd_buff_structs[i - 1].fd;
      pollfd_array[i].events = (fd_buff_structs[i - 1].state == STATE_REQ) ? POLLIN : POLLOUT;
      pollfd_array[i].events |= POLLERR;
      pollfd_array[i].revents = 0;
      *nfds += 1;
    }
  }
  return 0;
}

void serv_accept_connection(int serv_fd, fd_buff_struct_t *fd_buff_structs, size_t fd_buff_structs_size)
{
  int conn_fd = 0;
  struct sockaddr_in cli;
  int addrlen = sizeof(cli);

  /* Accept all queued connections */
  do {
    conn_fd = accept(serv_fd, (struct sockaddr *)&cli, &addrlen);
    if (conn_fd < 0)
      break;
    fd_set_non_blocking(conn_fd);
    fprintf(stdout, "Accepted %s\n", inet_ntoa(cli.sin_addr));
    /* Save in first vacant */
    for (size_t i = 0; i < fd_buff_structs_size; ++i) {
      if (fd_buff_structs[i].fd <= 0) {
        fd_buff_structs[i].fd = conn_fd;
        fd_buff_structs[i].state = STATE_REQ;

        fd_buff_structs[i].rbuff.buff_size = 0;
        fd_buff_structs[i].wbuff.buff_size = 0;
        fd_buff_structs[i].wbuff_sent = 0;
        break;
      }
    }
  } while (conn_fd != -1);
}
