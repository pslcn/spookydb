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
#include <errno.h>

#include "spkdb_http.h"

#define LENGTH(X) (sizeof X / sizeof X[0])

#define CAPACITY 50000

typedef struct {
	char *key, *value;
	size_t key_size, value_size;
} ht_item_t;

typedef struct {
	ht_item_t **items;
	int size, count;
} htable_t;

static void hash_func(unsigned long *hash, char *str) 
{
	unsigned long i = 0;

	for (size_t j = 0; str[j]; ++j) 
		i += str[j];
	*hash = i % CAPACITY; 
}

static void create_ht_item(ht_item_t **item, char *key, char *value)
{
	*item = malloc(sizeof(ht_item_t));
	(*item)->key = malloc(sizeof(char) * (strlen(key) + 1));
	(*item)->value = malloc(sizeof(char) * (strlen(value) + 1));
	strncpy((*item)->key, key, strlen(key));
	strncpy((*item)->value, value, strlen(value));

	(*item)->key_size = sizeof(char) * (strlen(key) + 1);
	(*item)->value_size = sizeof(char) * (strlen(value) + 1);

	fprintf(stdout, "[ht_item_t %p]: [Key (%ld bytes, at %p): '%s'] [Value (%ld bytes, at %p): '%s']\n", *item, (*item)->key_size, (*item)->key, (*item)->key, (*item)->value_size, (*item)->value, (*item)->value);
}

static void create_htable(htable_t **table)
{
	*table = malloc(sizeof(htable_t));
	(*table)->size = CAPACITY;
	(*table)->count = 0;
	(*table)->items = calloc((*table)->size, sizeof(ht_item_t*));

	for (size_t i = 0; i < (*table)->size; ++i)
		(*table)->items[i] = NULL;
}

static void free_ht_item(ht_item_t *item)
{
	free(item->key);
	free(item->value);
	free(item);
}

static void free_htable(htable_t *table)
{
	ht_item_t *item;
	for (size_t i = 0; i < table->size; ++i) {
		item = table->items[i];
		if (item != NULL)
			free_ht_item(item);
	}
	free(table->items);
	free(table);
}

static void handle_ht_collision(htable_t *table, ht_item_t *item)
{
	fprintf(stdout, "Collision!\n");
}

static void htable_insert(htable_t *table, char *key, char *value)
{
	fprintf(stdout, "(htable_insert) Inserting value '%s' in key '%s'\n", value, key); 
	ht_item_t *item, *current;
	unsigned long idx;
	create_ht_item(&item, key, value);
	hash_func(&idx, key);
	current = table->items[idx];
	if (current == NULL) {
		if (table->count == table->size) {
			fprintf(stdout, "Table is full!\n");
			free_ht_item(item);
			return;
		}
		/* fprintf(stdout, "(htable_insert) Storing item at %p in table->items[%d]\n", item, idx); */
		table->items[idx] = item;
		table->count++;

	} else if (strncmp(current->key, key, strlen(key)) == 0) {
		strncpy(table->items[idx]->value, value, strlen(value));
	} else {
		handle_ht_collision(table, item);
	}
}

static int htable_remove(htable_t *table, char *key)
{
	unsigned long idx;
	ht_item_t *current;
	hash_func(&idx, key);
	current = table->items[idx];

	fprintf(stdout, "Deleting key '%s' at %p\n", key, current);

	if ((current != NULL) && (strncmp(current->key, key, strlen(key)) == 0)) {
		table->items[idx] = NULL;
		table->count--;
		free_ht_item(current);
		return 0;
	} else 
		return 1;
}

static char *htable_search(htable_t *table, char *key) 
{
	unsigned long idx;
	ht_item_t *item;

	hash_func(&idx, key);
	item = table->items[idx];

	fprintf(stdout, "(htable_search) Searching %p in table->items[%ld] for key '%s'\n", item, idx, key);

	if (item == NULL)
		return NULL;

	fprintf(stdout, "(htable_search) Has key with %ld bytes\n", item->key_size);
	fprintf(stdout, "(htable_search) Has value with %ld bytes\n", item->value_size);

	if ((item->key_size > 0) && (item->value_size > 0)) {
		fprintf(stdout, "(htable_search) Searching key at %p\n", item->key);

		if (strncmp(item->key, key, strlen(key)) == 0) {
			fprintf(stdout, "(htable_search) Found value '%s' in key '%s'\n", item->value, item->key);

			return item->value;
		} 
	}

	return NULL;
}

/* ("tid=%p", pthread_self()) */

static htable_t *ht;

/* DB */
void http_handle_req(fd_buff_struct_t *fd_conn, parsed_http_req_t *parsed_http_req)
{
	ssize_t rv = 0;

	fd_conn->rbuff_size = 0;

	/*
	do {
		rv = read(fd_conn->fd, &fd_conn->rbuff[fd_conn->rbuff_size], fd_conn->rbuff_capacity - fd_conn->rbuff_size);
	} while (rv < 0 && errno == EINTR);

	if (rv < 0) 
		return;

	fd_conn->rbuff_size += (size_t)rv;
	*/

	read(fd_conn->fd, fd_conn->rbuff, fd_conn->rbuff_capacity);
	fd_conn->rbuff_size = fd_conn->rbuff_capacity;

	http_parse_req(fd_conn->rbuff, parsed_http_req->req_method, parsed_http_req->req_path, parsed_http_req->req_body, fd_conn->rbuff_size);
	fprintf(stdout, "METHOD: %s PATH: %s BODY: %s\n", parsed_http_req->req_method, parsed_http_req->req_path, parsed_http_req->req_body);

	/* Handle request method */
	char resp[RESP_LEN];

	/* Indexing req_path at 1 excludes '/' */
	if (strncmp(parsed_http_req->req_method, "GET", 4) == 0) {
		char *value_pointer = htable_search(ht, &parsed_http_req->req_path[1]), *resp_body;

		if (value_pointer != NULL) {
			resp_body = value_pointer;
		} else {
			resp_body = "NULL\n";
		}

		http_write_resp(resp, "200 OK", "Content-Type: text/plain", resp_body);

	} else if (strncmp(parsed_http_req->req_method, "PUT", 4) == 0) {
		htable_insert(ht, &parsed_http_req->req_path[1], parsed_http_req->req_body);
		http_write_resp(resp, "200 OK", "Content-Type: text/plain", "");

	} else if (strncmp(parsed_http_req->req_method, "DELETE", 7) == 0) {
		htable_remove(ht, &parsed_http_req->req_path[1]); 
		http_write_resp(resp, "200 OK", "Content-Type: text/plain", "");
	}

	size_t resp_bytes = 0;
	for (size_t i = 0; i < LENGTH(resp); ++i) {
		if (resp[i] == '\0') {
			resp_bytes = i;
			break;
		}	
	}

	memcpy(fd_conn->wbuff, &resp, resp_bytes);
	fd_conn->wbuff_size = resp_bytes;

	fd_conn->state = STATE_RES;
	http_handle_res(fd_conn);
}

/* Event loop uses poll.h */
int main(int argc, char *argv[])
{
	/* Command line options */
#if 0
	if (!(argc > 1))
		exit(1);

	int option;
	char *log_file_path = NULL;

	while ((option = getopt(argc, argv, ":l:")) != -1) {
		switch (option) {
			case 'l':
				log_file_path = optarg;
				break;
			case ':':
				exit(1);
			default: 
				exit(1);
		}
	}

	if (log_file_path != NULL)
		fprintf(stdout, "Logging to '%s'\n", log_file_path);
#endif

	int serv_fd;
	struct sockaddr_in servaddr;

	struct pollfd net_fds[NUM_CONNECTIONS];
	size_t nfds = 1;
	fd_buff_struct_t net_fd_buffs[NUM_CONNECTIONS];
	parsed_http_req_t parsed_http_reqs[NUM_CONNECTIONS];

	for (size_t i = 0; i < NUM_CONNECTIONS; ++i) {
		create_fd_buff_struct(&net_fd_buffs[i], BUFFSIZE, BUFFSIZE);
		create_parsed_http_req(&parsed_http_reqs[i]);
	}

	if (create_serv_sock(&serv_fd, &servaddr, 8080) != 0)
		exit(1);
	if (listen(serv_fd, (int)(NUM_CONNECTIONS / 2)) < 0)
		exit(1);
	net_fds[0].fd = serv_fd;
	net_fds[0].events = POLLIN;

	create_htable(&ht);
	htable_insert(ht, "Hello", "World");

	/* Event loop */
	while (1) {
		/* Blocks until pollfd array has been prepared */
		if (prepare_pollfd_array(net_fd_buffs, net_fds + 1, NUM_CONNECTIONS, &nfds) == 0) {
			if (poll(net_fds, nfds, 5000) > 0) {
				/* Check active connections */
				for (size_t i = 1; i < NUM_CONNECTIONS; ++i) {
					if (net_fds[i].fd > 0 && net_fds[i].revents) {
						/* Handle connection */
						if (net_fd_buffs[i - 1].state == STATE_REQ) {
							net_fd_buffs[i - 1].rbuff_size = 0;
							http_handle_req(&net_fd_buffs[i - 1], &parsed_http_reqs[i - 1]);
						} else if (net_fd_buffs[i - 1].state == STATE_RES)
							http_handle_res(&net_fd_buffs[i - 1]);

						/* Clean up array */
						if (net_fd_buffs[i - 1].fd > 0 && net_fd_buffs[i - 1].state == STATE_END) {
							fprintf(stdout, "%p: Closing FD %d in net_fd_buffs[%ld]\n", net_fd_buffs + (i - 1), net_fd_buffs[i - 1].fd, i - 1);

							close(net_fd_buffs[i - 1].fd);
							net_fd_buffs[i - 1].fd = 0;
						}
					} 
				}

				/* Check listening socket for connections to accept */
				if (net_fds[0].revents & POLLIN) {
					serv_accept_connection(serv_fd, net_fd_buffs, NUM_CONNECTIONS);
				}
			}
		}
	}

	/* Close FDs */
	fprintf(stdout, "Closing listening socket FD %d\n", serv_fd);
	close(serv_fd);
	for (size_t i = 0; i < NUM_CONNECTIONS; ++i) {
		if (net_fd_buffs[i].fd > 0) {
			fprintf(stdout, "Closing FD %d in net_fd_buffs[%ld]\n", net_fd_buffs[i].fd, i);
			close(net_fd_buffs[i].fd);
		}
	}
	for (size_t i = 0; i < NUM_CONNECTIONS; ++i) 
		close_fd_buff_struct(&net_fd_buffs[i]);
	free(net_fd_buffs);
	free_htable(ht);

	return 0;
}

