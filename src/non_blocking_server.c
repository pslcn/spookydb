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

int create_buff(struct rw_buff *buff, size_t buff_capacity)
{
  buff->buff_capacity = buff_capacity;
  buff->buff_content = calloc(buff_capacity, sizeof(char));
  return 0;
}

void clear_rw_buff(struct rw_buff *buff)
{
  buff->buff_size = 0;
  memset(buff->buff_content, 0, buff->buff_capacity);
}

int create_fd_conn_buffs(struct fd_conn_buffs *fd_buffs, size_t buff_capacity)
{
  create_buff(&fd_buffs->rbuff, buff_capacity);
  create_buff(&fd_buffs->wbuff, buff_capacity);
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


#if 0
void fd_buff_write_content(struct fd_buff_handler *fd_conn)
{
  ssize_t rv = 0;
  fd_conn->wbuff_sent = 0;

  do {
    rv = send(fd_conn->fd, fd_conn->wbuff.buff_content, fd_conn->wbuff.buff_size - fd_conn->wbuff_sent, 0);

    if (rv != -1) {
      fd_conn->wbuff_sent += rv;
    } else {
      if (errno == EAGAIN) {
        break;
      }

      fprintf(stderr, "[fd_buff_write_content] Error writing to FD: %s\n", strerror(errno));
    }
  } while (rv > 0);

  fprintf(stdout, "Sent response of %ld bytes to FD %d\n", fd_conn->wbuff_sent, fd_conn->fd);
  fd_conn->state = STATE_END;
}

void fd_buff_buffered_read(struct fd_buff_handler *fd_conn)
{
  ssize_t bytes_read = 0;

  do {
    bytes_read = recv(fd_conn->fd, fd_conn->rbuff.buff_content, fd_conn->rbuff.buff_capacity - fd_conn->rbuff.buff_size, 0);

    if (bytes_read != -1) {
      fd_conn->rbuff.buff_size += bytes_read;
    } else {
      if (errno == EAGAIN) {
        fprintf(stderr, "[fd_buff_buffered_read] EAGAIN\n");
        break;
      }

      fprintf(stderr, "[fd_buff_buffered_read] Error reading from FD:  %s\n", strerror(errno));
    }
  } while (bytes_read > 0);
}
#endif

