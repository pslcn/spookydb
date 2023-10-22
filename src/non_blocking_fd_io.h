#ifndef NON_BLOCKING_FD_IO_H_
#define NON_BLOCKING_FD_IO_H_

enum {
  STATE_REQ = 0,
  STATE_RES = 1,
  STATE_END = 2,
};

struct rw_buff {
  size_t buff_capacity, buff_size;
  char *buff_content;
};

struct fd_buff_handler {
  int fd;
  uint32_t state;

  struct rw_buff rbuff, wbuff;
  size_t wbuff_sent;
};

int create_buff(struct rw_buff *buff, size_t buff_capacity);
int create_fd_buff_handler(struct fd_buff_handler *fd_buff_handler, size_t rbuff_capacity, size_t wbuff_capacity);
void fd_buff_write_content(struct fd_buff_handler *fd_conn);
void fd_buff_buffered_read(struct fd_buff_handler *fd_conn);

int fd_set_non_blocking(int fd);
int create_serv_sock(int *serv_fd, struct sockaddr_in *servaddr, int port);


int prepare_pollfd_array(struct fd_buff_handler *fd_buff_handlers, struct pollfd *pollfd_array, size_t pollfd_array_size, size_t *nfds);
void serv_accept_connection(int serv_fd, struct fd_buff_handler *fd_buff_handlers, size_t fd_buff_handlers_size);

#endif

