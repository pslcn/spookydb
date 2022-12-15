#ifndef SERVER_H_
#define SERVER_H_

#include <stdbool.h>
#include <stddef.h>

typedef struct t_work t_work_t;
typedef struct t_work_queue t_work_queue_t;
typedef void (*thread_func_t)(void *arg);

typedef struct ht_item ht_item_t;
typedef struct htable htable_t;

static t_work_t *t_work_create(thread_func_t func, void *arg);
static void t_work_free(t_work_t *work);
static t_work_t *t_work_get(t_work_queue_t *work_queue);
static void *t_worker(void *arg);
t_work_queue_t *tpool_create(void);
bool t_work_new(t_work_queue_t *work_queue, thread_func_t func, void *arg);
void tpool_wait(t_work_queue_t *work_queue);
void tpool_destroy(t_work_queue_t *work_queue);

unsigned long hash_func(char *str);
ht_item_t *create_ht_item(char *key, char *value);
htable_t *create_htable(void);
void free_ht_item(ht_item_t *item);
void free_htable(htable_t *table);
void handle_ht_collision(htable_t *table, ht_item_t *item);
void htable_insert(htable_t *table, char *key, char *value);
int htable_remove(htable_t *table, char *key);
char *htable_search(htable_t *table, char *key);

void write_response(int connfd, char status[], char reponse_headers[], char response_body[]);
void handle_req(int *connfd);
void serve(void);

#endif
