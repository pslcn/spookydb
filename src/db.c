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

#include "net_threads.h"

#define BUFFSIZE 1024
#define CAPACITY 50000

/* Temporary hash table implementation */
typedef struct {
	char *key, *value;
} ht_item_t;

typedef struct {
	ht_item_t **items;
	int size, count;
} htable_t;

void hash_func(unsigned long *hash, char *str) 
{
	unsigned long i = 0;
	int j;
	for (j = 0; str[j]; j++) 
		i += str[j];
	*hash = i % CAPACITY; 
}

void create_ht_item(ht_item_t **item, char *key, char *value)
{
	*item = malloc(sizeof(ht_item_t));
	(*item)->key = malloc(strlen(key) + 1);
	(*item)->value = malloc(strlen(value) + 1);
	/* TODO: Fix vulnerabilities */
	strcpy((*item)->key, key);
	strcpy((*item)->value, value);
}

void create_htable(htable_t **table)
{
	int i;
	*table = malloc(sizeof(htable_t));
	(*table)->size = CAPACITY;
	(*table)->count = 0;
	(*table)->items = calloc((*table)->size, sizeof(ht_item_t*));
	for (i = 0; i < (*table)->size; ++i)
		(*table)->items[i] = NULL;
}

void free_ht_item(ht_item_t *item)
{
	free(item->key);
	free(item->value);
	free(item);
}

void free_htable(htable_t *table)
{
	ht_item_t *item;
	int i;
	for (i = 0; i < table->size; ++i) {
		item = table->items[i];
		if (item != NULL)
			free_ht_item(item);
	}
	free(table->items);
	free(table);
}

void handle_ht_collision(htable_t *table, ht_item_t *item)
{
	printf("Collision!\n");
}

void htable_insert(htable_t *table, char *key, char *value)
{
	printf("Inserting value '%s' in key '%s'\n", value, key);
	ht_item_t *item, *current;
	int idx;
	create_ht_item(&item, key, value);
	hash_func(&idx, key);
	current = table->items[idx];
	if (current == NULL) {
		if (table->count == table->size) {
			printf("Table is full!\n");
			free_ht_item(item);
			return;
		}
		table->items[idx] = item;
		table->count++;
	} else if (strcmp(current->key, key) == 0) {
		strcpy(table->items[idx]->value, value);
	} else {
		handle_ht_collision(table, item);
	}
}

int htable_remove(htable_t *table, char *key)
{
	printf("Deleting key '%s'\n", key);
	int idx;
	ht_item_t *current;
	hash_func(&idx, key);
	current = table->items[idx];
	if (current != NULL & (strcmp(current->key, key) == 0)) {
		table->items[idx] = NULL;
		table->count--;
		free_ht_item(current);
		return 0;
	} else {
		return 1;
	}
}

char *htable_search(htable_t *table, char *key) 
{
	int idx;
	ht_item_t *item;
	hash_func(&idx, key);
	item = table->items[idx];
	if (item != NULL && (strcmp(item->key, key) == 0)) 
		return item->value;
	return NULL;
}

/* Basic net responses */
void write_resp(int connfd, char status[], char resp_headers[], char resp_body[])
{
	char *resp = ("HTTP/1.1 %s\n%s\nContent-Length: %d\n\n%s", status, resp_headers, strlen(resp_body), resp_body); 
	write(connfd, resp, strlen(resp));
}

htable_t *ht;

void handle_req(int *connfd)
{
	if (*connfd < 0) 
		exit(1);
	char req[BUFFSIZE], *req_method, *req_path, *req_query, *resp_body;
	char firstline[80];
	int c;
	char formatted_path[40];
	char *val;
	char req_body[BUFFSIZE];
	int isbody = 0;
	bzero(req, BUFFSIZE);
	read(*connfd, req, BUFFSIZE - 1);
	for (c = 0; c < 80; ++c) {
		firstline[c] = *(req + c);
		if (*(req + c) == '\n')
			break;
	}
	req_method = strtok(firstline, " ");
	req_path = strtok(strtok(NULL, " "), "?");
	for (c = 1; c < strlen(req_path); ++c) {
		formatted_path[c - 1] = req_path[c];
	}
	req_path = formatted_path;
	req_query = strtok(NULL, "?");
	if (strcmp(req_method, "GET") == 0) {
		val = htable_search(ht, req_path);
		if (val != NULL) {
			resp_body = val;
		} else {
			resp_body = "NULL";
		}
	} else if (strcmp(req_method, "PUT") == 0) {
		for (c = 0; c < strlen(req); ++c) {
			if (isbody == 0) {
				if (*(req + c) == '\n' && *(req + c + 2) == '\n') {
					c += 2;
					isbody = c + 1;
				}
			} else {
				req_body[c - isbody] = *(req + c);
			}
		}
		htable_insert(ht, req_path, req_body);
		resp_body = ("tid=%p method=PUT", pthread_self());
	} else if (strcmp(req_method, "DELETE") == 0) {
		htable_remove(ht, req_path);
		resp_body = ("tid=%p method=DELETE", pthread_self());
	}
	write_resp(*connfd, "200 OK", "Content-Type: text/plain", resp_body);
	close(*connfd);
}

/* Temporary */
sig_atomic_t keep_serving = 1;

void term_handler(int signum)
{
	keep_serving = 0;
}

static void
start_daemon(void)
{
	pid_t pid = fork();
	if(pid < 0) exit(1);
	if(pid > 0) exit(0);
	if(setsid() < 0) exit(1);
	signal(SIGTERM, term_handler);
	pid = fork();
	if(pid < 0) exit(1);
	if(pid > 0) exit(0);
	umask(0);
	for(int x = sysconf(_SC_OPEN_MAX); x >= 0; x--)	close(x);
}

/* DB */
int main(void)
{
	// start_daemon();
	int sockfd, addr_len;
	struct sockaddr_in servaddr, cli;
	t_work_queue_t *work_queue;
	htable_t *test_ht;
	int *conns;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1)
		exit(1);
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(8080);
	if ((bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) 
		exit(1);
	t_pool_create(&work_queue);
	if (listen(sockfd, 4) < 0) /* NTHREADS = 4 */
		exit(1);
	addr_len = sizeof(cli);
	create_htable(&test_ht);
	htable_insert(test_ht, "Hello", "World");
	ht = test_ht;
	conns = malloc(4 * sizeof(int*)); /* NTHREADS = 4 */
	while (keep_serving) {
		conns[0] = accept(sockfd, (struct sockaddr *)&cli, &addr_len);
		t_work_new(work_queue, handle_req, conns);
	}
	t_pool_wait(work_queue);
	free(conns);
	t_pool_destroy(work_queue);
	free_htable(test_ht);
	close(sockfd);
	return 0;
}

