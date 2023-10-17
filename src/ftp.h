#ifndef FTP_H_
#define FTP_H_

typedef struct {
  int serv_fd21, serv_fd20;
  struct sockaddr_in servaddr21, servaddr20;

  size_t num_poll_fds;
  struct pollfd *pollfd_array21, *pollfd_array20;
  struct fd_buff_handler *pollfd_buffs21, *pollfd_buffs20;
} ftp_handler_t;

int create_ftp_handler(ftp_handler_t *ftp_handler);

#endif

