#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <poll.h>
#include <fcntl.h> 
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>

#include "non_blocking_fd_io.h"
#include "ftp.h"

#define LENGTH(X) (sizeof X / sizeof X[0])

#define BUFFSIZE 1024
#define NUM_CONNECTIONS 32

int create_ftp_handler(ftp_handler_t *ftp_handler)
{
  if (create_serv_sock(&(ftp_handler->serv_fd21), &(ftp_handler->servaddr21), 21) != 0)
    return 1;
  if (create_serv_sock(&(ftp_handler->serv_fd20), &(ftp_handler->servaddr20), 20) != 0)
    return 1;
  if (listen(ftp_handler->serv_fd21, (int)(NUM_CONNECTIONS / 2)) < 0)
    return 1;
  if (listen(ftp_handler->serv_fd20, (int)(NUM_CONNECTIONS / 2)) < 0)
    return 1;

  ftp_handler->num_poll_fds = NUM_CONNECTIONS;
  ftp_handler->pollfd_array21 = malloc(sizeof(struct pollfd) * ftp_handler->num_poll_fds);
  ftp_handler->pollfd_array20 = malloc(sizeof(struct pollfd) * ftp_handler->num_poll_fds);
  fprintf(stdout, "Allocated %zu bytes for pollfd_array21 and pollfd_array20\n", sizeof(struct pollfd) * ftp_handler->num_poll_fds * 2);
  ftp_handler->pollfd_array21[0].fd = ftp_handler->serv_fd21;
  ftp_handler->pollfd_array21[0].events = POLLIN;

  ftp_handler->pollfd_buffs21 = malloc(sizeof(struct fd_buff_handler) * ftp_handler->num_poll_fds);
  ftp_handler->pollfd_buffs20 = malloc(sizeof(struct fd_buff_handler) * ftp_handler->num_poll_fds);
  fprintf(stdout, "Allocated %zu bytes for pollfd_buffs21 and pollfd_buffs20\n", sizeof(struct fd_buff_handler) * ftp_handler->num_poll_fds * 2);
  for (size_t i = 0; i < ftp_handler->num_poll_fds; ++i) {
    create_fd_buff_struct(&(ftp_handler->pollfd_buffs21[i]), BUFFSIZE, BUFFSIZE);
    create_fd_buff_struct(&(ftp_handler->pollfd_buffs20[i]), BUFFSIZE, BUFFSIZE);
  }
  return 0;
}

void serve(ftp_handler_t *ftp_handler)
{
 size_t nfds = 1; 

  while (1) {
    if (prepare_pollfd_array(ftp_handler->pollfd_buffs21, &(ftp_handler->pollfd_array21[1]), NUM_CONNECTIONS, &nfds) == 0) {
      if (poll(ftp_handler->pollfd_array21, nfds, 5000) > 0) {
        for (size_t i = 1; i < ftp_handler->num_poll_fds; ++i) {
          if (ftp_handler->pollfd_array21[i].fd > 0 && ftp_handler->pollfd_array21[i].revents) {
            if (ftp_handler->pollfd_array21[i - 1].fd > 0 && ftp_handler->pollfd_buffs21[i - 1].state == STATE_END) {
              close(ftp_handler->pollfd_array21[i - 1].fd);
              ftp_handler->pollfd_array21[i - 1].fd = 0;
            }
          }
        }

        if (ftp_handler->pollfd_array21[0].revents & POLLIN) {
          serv_accept_connection(ftp_handler->serv_fd21, ftp_handler->pollfd_buffs21, ftp_handler->num_poll_fds);
        }
      }
    }
  }
}

