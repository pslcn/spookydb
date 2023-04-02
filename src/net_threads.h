#ifndef NET_THREADS_H_
#define NET_THREADS_H_

#include <stdbool.h>

typedef void (*thread_func_t)(void *arg);
typedef struct t_work_queue t_work_queue_t;

bool t_work_new(t_work_queue_t *work_queue, thread_func_t func, void *arg);
void t_pool_create(t_work_queue_t **work_queue);
void t_pool_wait(t_work_queue_t *work_queue);
void t_pool_destroy(t_work_queue_t *work_queue);

#endif
