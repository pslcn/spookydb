#ifndef NET_THREADS_H_
#define NET_THREADS_H_

typedef struct t_work t_work_t;
typedef struct t_work_queue t_work_queue_t;
typedef void (*thread_func_t)(void *arg);

static void t_work_create(t_work_t **work, thread_func_t func, void*arg);
static void t_work_free(t_work_t *work);
static void t_work_get(t_work_t **work, t_work_queue_t *work_queue);
static void *t_worker(void *arg);
void t_pool_create(t_work_queue_t **work_queue);
bool t_work_new(t_work_queue_t *work_queue, thread_func_t func, void *arg);
void t_pool_wait(t_work_queue_t *work_queue);
void t_pool_destroy(t_work_queue_t *work_queue);

void term_handler(int signum);
static void start_daemon(void);
void serve(void);

#endif
