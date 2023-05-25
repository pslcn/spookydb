/* Non-Blocking FD I/O */

#ifndef NB_FD_IO_H_
#define NB_FD_IO_H_

enum {
	STATE_REQ = 0,
	STATE_RES = 1,
	STATE_END = 2,
};

typedef struct fd_buff_struct fd_buff_struct_t;

int create_fd_buff_struct(fd_buff_struct_t *fd_buff, size_t rbuff_size, size_t wbuff_capacity);
int close_fd_buff_struct(fd_buff_struct_t *fd_buff);

int fd_set_non_blocking(int fd);
int create_serv_sock(int *serv_fd, struct sockaddr_in *servaddr, int port);

void serv_accept_connection(int serv_fd, fd_buff_struct_t *fd_buff_structs, size_t fd_buff_structs_size);
void prepare_pollfd_array(fd_buff_struct_t *fd_buff_structs, struct pollfd **pollfd_array, size_t pollfd_array_size, size_t *nfds);

#endif
