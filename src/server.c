#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <sys/time.h>
#include <pthread.h>

#include "server.h"

#define NTHREADS 4
#define BUFFSIZE 1024
#define CAPACITY 50000

typedef struct t_work {
	thread_func_t func;
	void *arg;
	struct t_work *next;
} t_work_t;

typedef struct t_work_queue {
	t_work_t *first, *last;
	pthread_mutex_t mtx;
	pthread_cond_t work_cond, busy_cond;
	size_t num_working, num_active;
	bool stop;
} t_work_queue_t;

typedef struct ht_item {
	char *key, *value;
} ht_item_t;

typedef struct htable {
	ht_item_t **items;
	int size, count;
} htable_t;

static t_work_t 
*t_work_create(thread_func_t func, void *arg)
{
	t_work_t *work;
	if(func == NULL) return NULL;
	work = malloc(sizeof(work));
	work->func = func;
	work->arg = arg;
	work->next = NULL;
	return work;
}

static void 
t_work_free(t_work_t *work) 
{
	if(work == NULL) return;
	free(work);
}

static t_work_t 
*t_work_get(t_work_queue_t *work_queue) 
{
	t_work_t *work;
	if(work_queue == NULL) return NULL;
	work = work_queue->first;
	if(work == NULL) return NULL;
	if(work->next == NULL) {
		work_queue->first = NULL;
		work_queue->last = NULL;
	} else {
		work_queue->first = work->next;
	}
	return work;
}

static void 
*t_worker(void *arg) 
{
	t_work_queue_t *work_queue = arg;
	t_work_t *work;
	while(1) {
		pthread_mutex_lock(&(work_queue->mtx));
		while(work_queue->first == NULL && !work_queue->stop) pthread_cond_wait(&(work_queue->work_cond), &(work_queue->mtx));
		if(work_queue->stop) break;
		work = t_work_get(work_queue);
		work_queue->num_working++;
		pthread_mutex_unlock(&(work_queue->mtx));
		if(work != NULL) {
			work->func(work->arg);
			t_work_free(work);
		}
		pthread_mutex_lock(&(work_queue->mtx));
		work_queue->num_working--;
		if(!work_queue->stop && work_queue->num_working == 0 && work_queue->first == NULL) pthread_cond_signal(&(work_queue->busy_cond));
		pthread_mutex_unlock(&(work_queue->mtx));
	}
	work_queue->num_active--;
	pthread_cond_signal(&(work_queue->busy_cond));
	pthread_mutex_unlock(&(work_queue->mtx));
	return NULL;
}

t_work_queue_t 
*tpool_create(void)
{
	t_work_queue_t *work_queue;
	pthread_t thread;
	work_queue = calloc(1, sizeof(*work_queue));
	work_queue->num_active = NTHREADS;	
	pthread_mutex_init(&(work_queue->mtx), NULL);
	pthread_cond_init(&(work_queue->work_cond), NULL);
	pthread_cond_init(&(work_queue->busy_cond), NULL);
	work_queue->first = NULL;
	work_queue->last = NULL;
	for(int t=0; t<NTHREADS; t++) {
		pthread_create(&thread, NULL, t_worker, work_queue);	
		pthread_detach(thread);
	}
	return work_queue;
}

bool 
t_work_new(t_work_queue_t *work_queue, thread_func_t func, void *arg) 
{
	t_work_t *work;
	if(work_queue == NULL) return false;
	work = t_work_create(func, arg);
	if(work == NULL) return false;	
	pthread_mutex_lock(&(work_queue->mtx));
	if(work_queue->first == NULL) {
		work_queue->first = work;
		work_queue->last = work_queue->first;
	} else {
		work_queue->last->next = work;
		work_queue->last = work;
	}
	pthread_cond_broadcast(&(work_queue->work_cond));
	pthread_mutex_unlock(&(work_queue->mtx));
	return true;
}

void
tpool_wait(t_work_queue_t *work_queue)
{
	printf("tid=%p\n", pthread_self());
	if(work_queue == NULL) return;
	pthread_mutex_lock(&(work_queue->mtx));
	while(1) {
		if((!work_queue->stop && work_queue->num_working != 0) || (work_queue->stop && work_queue->num_active != 0)) {
			pthread_cond_wait(&(work_queue->busy_cond), &(work_queue->mtx));
		} else {
			break;
		}
	}
	pthread_mutex_unlock(&(work_queue->mtx));
}

void
tpool_destroy(t_work_queue_t *work_queue)
{
	t_work_t *work;
	t_work_t *work2;
	if(work_queue == NULL) return;
	pthread_mutex_lock(&(work_queue->mtx));
	work = work_queue->first;
	while(work != NULL) {
		work2 = work->next;
		t_work_free(work);
		work = work2;
	}
	work_queue->stop = true;
	pthread_cond_broadcast(&(work_queue->work_cond));
	pthread_mutex_unlock(&(work_queue->mtx));
	tpool_wait(work_queue);
	pthread_mutex_destroy(&(work_queue->mtx));
	pthread_cond_destroy(&(work_queue->work_cond));
	pthread_cond_destroy(&(work_queue->busy_cond));
	free(work_queue);
}

unsigned long 
hash_func(char *str)
{
	unsigned long i = 0;
	for(int j = 0; str[j]; j++) i += str[j];
	return i % CAPACITY;
}

ht_item_t 
*create_ht_item(char *key, char *value)
{
	ht_item_t *item = (ht_item_t *)malloc(sizeof(ht_item_t));
	item->key = (char *)malloc(strlen(key) + 1);
	item->value = (char *)malloc(strlen(value) + 1);
	strcpy(item->key, key);
	strcpy(item->value, value);
	return item;
}

htable_t
*create_htable(void)
{
	htable_t *table = (htable_t *)malloc(sizeof(htable_t));
	table->size = CAPACITY;
	table->count = 0;
	table->items = (ht_item_t **)calloc(table->size, sizeof(ht_item_t*));
	for(int i = 0; i < table->size; i++) table->items[i] = NULL;
	return table;
}

void 
free_ht_item(ht_item_t *item)
{
	free(item->key);
	free(item->value);
	free(item);
}

void
free_htable(htable_t *table)
{
	for(int i = 0; i < table->size; i++) {
		ht_item_t *item = table->items[i];
		if(item != NULL) free_ht_item(item);
	}
	free(table->items);
	free(table);
}

void
handle_ht_collision(htable_t *table, ht_item_t *item)
{
	printf("Collision!");
}

void
htable_insert(htable_t *table, char *key, char *value)
{
	printf("Inserting value '%s' in key '%s'\n", value, key);
	ht_item_t *item = create_ht_item(key, value);
	int idx = hash_func(key);
	ht_item_t *current = table->items[idx];
	if(current == NULL) {
		if(table->count == table->size) {
			printf("Table is full!\n");
			free_ht_item(item);
			return;
		}
		table->items[idx] = item;
		table->count++;
	} else if(strcmp(current->key, key) == 0) {
		strcpy(table->items[idx]->value, value);
	} else {
		handle_ht_collision(table, item);
	}
}

char
*htable_search(htable_t *table, char *key)
{
	int idx = hash_func(key);
	ht_item_t *item = table->items[idx];
	if(item != NULL && (strcmp(item->key, key) == 0)) return item->value;
	return NULL;
}

void 
write_response(int connfd, char status[], char response_headers[], char response_body[])
{
	char *response = ("HTTP/1.1 %s\n%s\nContent-Length: %d\n\n%s", status, response_headers, strlen(response_body), response_body); 
	write(connfd, response, strlen(response));
}

htable_t *ht;

void 
handle_req(int *connfd)
{
	if(*connfd < 0) exit(1);

	char req[BUFFSIZE], *req_method, *req_path, *req_query, *resp_body;
	bzero(req, BUFFSIZE);
	read(*connfd, req, BUFFSIZE - 1);

	char firstline[80];
	for(int c = 0; c < 80; c++) {
		firstline[c] = *(req + c);
		if(*(req + c) == '\n') break;
	}
	req_method = strtok(firstline, " ");
	req_path = strtok(strtok(NULL, " "), "?");
	char formatted_path[40];
	for(int c = 1; c < strlen(req_path); c++) {
		formatted_path[c - 1] = req_path[c];
	}
	req_path = formatted_path;
	req_query = strtok(NULL, "?");

	if(strcmp(req_method, "GET") == 0) { 
		char *val = htable_search(ht, req_path);
		if(val != NULL) {
			resp_body = val;
		} else {
			resp_body = "NULL";
		}
	}	else if(strcmp(req_method, "PUT") == 0) { 
		char req_body[BUFFSIZE];
		int isbody = 0;
		for(int c = 0; c < strlen(req); c++) {
			if(isbody == 0) {
				if(*(req + c) == '\n' && *(req + c + 2) == '\n') {
					c += 2;
					isbody = c + 1;
				}
			
			} else {
				req_body[c - isbody] = *(req + c);
			}
		}

		htable_insert(ht, req_path, req_body);
		resp_body = ""; 
	} else if(strcmp(req_method, "DELETE") == 0) { 
		resp_body = ""; 
	}
	write_response(*connfd, "200 OK", "Content-Type: text/plain", resp_body);
	close(*connfd);
}

void 
serve(void)
{
	int sockfd, addr_len;
	struct sockaddr_in servaddr, cli;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd == -1) exit(1);
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(8080);
	if((bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) exit(1);

	t_work_queue_t *work_queue = tpool_create();
	if(listen(sockfd, NTHREADS) < 0) exit(1);
	addr_len = sizeof(cli);

	htable_t *test_ht = create_htable();
	htable_insert(test_ht, "Hello", "World");
	ht = test_ht;

	int *conns = (int *)malloc(2 * sizeof(int *));
	conns[0] = accept(sockfd, (struct sockaddr *)&cli, &addr_len);
	t_work_new(work_queue, handle_req, conns);
	conns[1] = accept(sockfd, (struct sockaddr *)&cli, &addr_len);
	t_work_new(work_queue, handle_req, conns + 1);

	// Something like the below would use a function returning a function pointer - meaning only one argument would be needed (the 'conn_fd').
	// t_work_new(work_queue, handle_req(test_ht), accept(sockfd, (struct sockaddr *)&cli, &addr_len));

	tpool_wait(work_queue);
	free(conns);
	tpool_destroy(work_queue);
	free_htable(test_ht);
	close(sockfd);
}

