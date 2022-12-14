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

typedef struct t_work {
	thread_func_t func;
	void *arg;
	struct t_work *next;
} t_work_t;

typedef struct t_work_queue {
	t_work_t *first;
	t_work_t *last;
	pthread_mutex_t mtx;
	pthread_cond_t work_cond;
	pthread_cond_t busy_cond;
	size_t num_working;
	size_t num_active;
	bool stop;
} t_work_queue_t;

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

/*
void 
write_response(int connfd, char status[], char response_headers[], char response_body[])
{
	char *response = ("HTTP/1.1 %s\n%s\nContent-Length: %d\n\n%s", status, response_headers, strlen(response_body), response_body); 
	write(connfd, response, strlen(response));
}

void 
*handle_req(int connfd)
{
	if(connfd < 0) exit(1);
	char req[BUFFSIZE], *req_method, *req_path, *req_query, *resp_body;
	bzero(req, BUFFSIZE);
	read(connfd, req, BUFFSIZE - 1);
	req_method = strtok(req, " ");
	req_path = strtok(strtok(NULL, " "), "?");
	req_query = strtok(NULL, "?");
	if(strcmp(req_method, "GET") == 0) { resp_body = "GET!"; }	
	else if(strcmp(req_method, "PUT") == 0) { resp_body = "PUT!"; }
	else if(strcmp(req_method, "DELETE") == 0) { resp_body = "DELETE"; }
	write_response(connfd, "200 OK", "Content-Type: text/html", resp_body);
	close(connfd);
}

void 
handle(int connfd)
{
	if(connfd < 0) exit(1);
	// Get a request
	char req[BUFFSIZE];
	bzero(req, BUFFSIZE);
	read(connfd, req, BUFFSIZE - 1);
	// Parse a request
	char *req_method, *req_path, *req_query;
	req_method = strtok(req, " ");
	req_path = strtok(strtok(NULL, " "), "?");
	req_query = strtok(NULL, "?");
	if(req_query != NULL) {
		printf("Request method: %s\nRequest path: %s\nRequest query: %s\n", req_method, req_path, req_query); 
	} else {
		printf("Request method: %s\nRequest path: %s\nRequest query: NULL\n", req_method, req_path);
	}
	// Handle a request
	char *response_body;
	if(strcmp(req_method, "GET") == 0) {
		response_body = "GET!";
	} else if(strcmp(req_method, "PUT") == 0) {
		response_body = "PUT!";
	} else if(strcmp(req_method, "DELETE") == 0) {
		response_body = "DELETE!";
	}
	write_response(connfd, "200 OK", "Content-Type: text/html", response_body);
	close(connfd);
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
	// if(listen(sockfd, NTHREADS) < 0) exit(1);
	listen(sockfd, NTHREADS);
	// addr_len = sizeof(cli);
	// handle(accept(sockfd, (struct sockaddr *)&cli, &addr_len));
	// Shutdown
	close(sockfd);
}
*/

// Testing
void 
worker(void *arg)
{
	int *val = arg;
	int old = *val;

	*val += 1000;
	printf("tid=%p, old=%d, val=%d\n", pthread_self(), old, *val);
	if(*val%2) usleep(100000);
}

int
main(int argc, char **argv)
{
	t_work_queue_t *work_queue;
	int *vals;
	work_queue = tpool_create();
	vals = calloc(100, sizeof(*vals));
	for(int i=0; i<100; i++) {
		vals[i] = i;
		t_work_new(work_queue, worker, vals+i);
	}
	tpool_wait(work_queue);
	for(int i=0; i<100; i++) {
		printf("%d\n", vals[i]);
	}
	free(vals);
	tpool_destroy(work_queue);
	return 0;
}

