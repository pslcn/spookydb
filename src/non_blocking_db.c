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
#include <poll.h>
#include <fcntl.h>
#include <arpa/inet.h>

#include "net_threads.h"

#define BUFFSIZE 1024
#define CAPACITY 50000
#define LENGTH(X) (sizeof X / sizeof X[0])

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

/* Basic network responses */
/*
void write_resp(int connfd, char status[], char resp_headers[], char resp_body[])
{
	char *resp = ("HTTP/1.1 %s\n%s\nContent-Length: %d\n\n%s", status, resp_headers, strlen(resp_body), resp_body);
	write(connfd, resp, strlen(resp));
}
*/

void write_resp(int conn_fd, char *status, char *resp_headers, char *resp_body)
{
	write(conn_fd, resp_body, strlen(resp_body));
}

htable_t *ht;

/*
int read_n_req_bytes(int connfd, char *buff, size_t n)
{
	size_t read_values;
	while (n > 0) {
		read_values = read(connfd, buff, n);
		if (read_values <= 0)
			return -1;
		n -= (size_t)read_values;
		buff += read_values;
	}
	return 0;
}
*/

void parse_req(char *req, char *r_method[], char *r_path[], char *r_body[])
{
	char rm[2048], rp[2048], rb[2048];
	int spaces, isbody = 0;
	int c;
	for (c = 0; c < 512; ++c) {
		if (spaces == 0) {
			if (req[c] != ' ') {
				rm[c] = req[c];
			} else {
				rm[c] = '\0';
				spaces = c + 1; 
			}
		} else {
			if (req[c] != ' ') {
				rp[c - spaces] = req[c];
			} else {
				rp[c - spaces] = '\0';
				break;
			}
		}
	}
	if (strcmp(rm, "PUT") == 0) {
		for (c = 0; c < BUFFSIZE; ++c) {
			if (isbody == 0) {
				if (req[c] == '\n' && req[c + 2] == '\n') {
					c += 2;
					isbody = c + 1;
				} 
			} else {
				rb[c - isbody] = req[c]; 
			}
		}
		rb[BUFFSIZE - isbody] = '\0';
	} else {
		strncpy(rb, "NULL", 5);
	}
	strcpy(r_method, rm);
	strcpy(r_path, rp);
	strcpy(r_body, rb);
}

void temp_handle_req(int *connfd)
{
	char req[BUFFSIZE];
	char *val;
	char r_method[2048], r_path[2048], r_body[2048];
	char *resp_body;
	bzero(req, BUFFSIZE);
	read(*connfd, req, BUFFSIZE - 1);
	parse_req(req, &r_method, &r_path, &r_body);

	/*
	if (strcmp(r_method, "GET") == 0) {
		val = htable_search(ht, r_path);	
		if (val != NULL) {
			resp_body = val;
		} else {
			resp_body = "NULL";
		}
	} else if (strcmp(r_method, "PUT") == 0) {
		htable_insert(ht, r_path, r_body);
		resp_body = ("tid=%p method=PUT", pthread_self());
	} else if (strcmp(r_method, "DELETE") == 0) {
		htable_remove(ht, r_path);
		resp_body = ("tid=%p method=DELETE", pthread_self());
	}
	*/

	// write_resp(*connfd, "200 OK", "Content-Type: text/plain", resp_body);
	write_resp(*connfd, "", "", pthread_self());
	close(*connfd);
	*connfd = -1;
}

void handle_req(int *conn_fd) 
{
	/*
	char req[BUFFSIZE];
	char r_method[8], r_path[2048], r_body[2048];

	bzero(req, BUFFSIZE);
	read(*conn_fd, req, BUFFSIZE);

	parse_req(req, &r_method, &r_path, &r_body);

	write_resp(*conn_fd, "200 OK", "Content-Type: text/plain", "Hello!");
	*/

	fprintf(stdout, "tid: %p conn_fd: %d at %p\n", pthread_self(), *conn_fd, conn_fd);

	close(*conn_fd);
	*conn_fd = -1;
}

int fd_set_non_blocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1) 
		return 1;
	flags |= O_NONBLOCK;

	return (fcntl(fd, F_SETFL, flags) == 0) ? 0 : 1;
}

/* Non-blocking server socket FD */
int create_serv_sock(int *serv_fd, struct sockaddr_in *servaddr) 
{
	*serv_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (*serv_fd == -1)
		return 1;
	fd_set_non_blocking(*serv_fd);

	bzero(servaddr, sizeof(*servaddr));
	(*servaddr).sin_family = AF_INET;
	(*servaddr).sin_addr.s_addr = htonl(INADDR_ANY);
	(*servaddr).sin_port = htons(8080);
	if ((bind(*serv_fd, (struct sockaddr *)servaddr, sizeof(*servaddr))) != 0)
		return 1;

	return 0;
}

/* Event loop uses poll.h */
int main(void) 
{
	int serv_fd;
	struct sockaddr_in servaddr;

	struct pollfd net_fds[32] = { 0 };
	int nfds = 1;
	int conn_fd = 0;
	t_work_queue_t *work_queue;

	if (create_serv_sock(&serv_fd, &servaddr) != 0) 
		exit(1);
	if (listen(serv_fd, 32) < 0)
		exit(1);

	net_fds[0].fd = serv_fd;
	net_fds[0].events = POLLIN;

	t_pool_create(&work_queue);

	create_htable(&ht);
	htable_insert(ht, "Hello", "World");

	while (1) {
		if (poll(net_fds, nfds, (5000)) > 0) {
			/* Accept connection */
			if (net_fds[0].revents & POLLIN) { 
				struct sockaddr_in cli;
				int addrlen = sizeof(cli);
				conn_fd = accept(serv_fd, (struct sockaddr *)&cli, &addrlen);
				if (conn_fd < 0)
					break;

				fprintf(stdout, "Accepted %s\n", inet_ntoa(cli.sin_addr));
				for (size_t i = 1; i < 32; ++i) {
					if (net_fds[i].fd <= 0) {
						fprintf(stdout, "Storing in net_fds[%d]\n", i);
						net_fds[i].fd = conn_fd;
						net_fds[i].events = POLLIN;
						nfds += 1;
						break;
					}
				}
			}

			/* Check existing connections */
			for (size_t i = 1; i < 32; ++i) {
				if (net_fds[i].fd > 0 && net_fds[i].revents & POLLIN) {
					fprintf(stdout, "POLLIN for net_fds[%d]\n", i);
					t_work_new(work_queue, &handle_req, &net_fds[i]);
				}
			}

			/* Clean up array */
			for (size_t i = 1; i < 32; ++i) {
				if (net_fds[i].fd < 0) {
					fprintf(stdout, "Cleaning net_fds[%d]\n", i);
					net_fds[i].fd = 0;
					net_fds[i].events = 0;
					net_fds[i].revents = 0;
					nfds -= 1;
				}
			}
		}
	}

	t_pool_wait(work_queue);
	t_pool_destroy(work_queue);

	/* Close FDs */
	for (size_t i = 0; i < 32; ++i) {
		if (net_fds[i].fd >= 0)
			close(net_fds[i].fd);
	}

	free_htable(ht);

	return 0;
}

