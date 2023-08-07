#ifndef NON_BLOCKING_FD_IO_H_
#define NON_BLOCKING_FD_IO_H_

enum {
  STATE_REQ = 0,
  STATE_RES = 1,
  STATE_END = 2,
};

typedef struct {
  size_t buff_capacity, buff_size;
  uint8_t *buff_content;
} buff_t;

typedef struct {
  int fd;
  uint32_t state;

  buff_t rbuff, wbuff;
  size_t wbuff_sent;
} fd_buff_struct_t;

int create_buff(buff_t *buff, size_t buff_capacity);
int create_fd_buff_struct(fd_buff_struct_t *fd_buff, size_t rbuff_capacity, size_t wbuff_capacity);

int fd_set_non_blocking(int fd);
int create_serv_sock(int *serv_fd, struct sockaddr_in *servaddr, int port);

int prepare_pollfd_array(fd_buff_struct_t *fd_buff_structs, struct pollfd *pollfd_array, size_t pollfd_array_size, size_t *nfds);
void serv_accept_connection(int serv_fd, fd_buff_struct_t *fd_buff_structs, size_t fd_buff_structs_size);

#endif

