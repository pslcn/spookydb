#ifndef DB_H_
#define DB_H_

#include <stdbool.h>
#include <stddef.h>

typedef struct t_work t_work_t;
typedef struct t_work_queue t_work_queue_t;
typedef void (*thread_func_t)(void *arg);

static t_work_t *t_work_create(thread_func_t func, void *arg);
static void t_work_free(t_work_t *work);
static t_work_t *t_work_get(t_work_queue_t *work_queue);
static void *t_worker(void *arg);
t_work_queue_t *tpool_create(void);
bool t_work_new(t_work_queue_t *work_queue, thread_func_t func, void *arg);
void tpool_wait(t_work_queue_t *work_queue);
void tpool_destroy(t_work_queue_t *work_queue);

void term_handler(int signum);
static void start_daemon(void);
void serve(void);

#endif
