#ifndef NON_BLOCKING_SERVER_H_
#define NON_BLOCKING_SERVER_H_

enum io_state {
    STATE_READY = 0,
    STATE_REQ = 1,
    STATE_RES = 2
};

struct rw_buff {
    size_t buff_capacity, buff_size;
    char *buff_content;
};

struct fd_conn_buffs {
    enum io_state state;
    struct rw_buff rbuff, wbuff;
    size_t wbuff_sent;
};

int create_buff(struct rw_buff *buff, size_t buff_capacity);
void clear_rw_buff(struct rw_buff *buff);
int create_fd_conn_buffs(struct fd_conn_buffs *fd_buffs, size_t buff_capacity);

int fd_set_non_blocking(int fd);
int create_serv_sock(int *serv_fd, struct sockaddr_in *servaddr, int port);

#endif

