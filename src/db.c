#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <sys/time.h>
#include <pthread.h>

#include "db.h"

#define NTHREADS 4

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

static t_work_t *t_work_create(thread_func_t func, void *arg) 
{
	t_work_t *work;
	if (func == NULL) 
		return NULL;
	work = malloc(sizeof(work));
	work->func = func;
	work->arg = arg;
	work->next = NULL;
	return work;
}

static void t_work_free(t_work_t *work) 
{
	if (work == NULL) 
		return;
	free(work);
}

static t_work_t *t_work_get(t_work_queue_t *work_queue)
{
	t_work_t *work;
	if (work_queue == NULL)
		return NULL;
	work = work_queue->first;
	if (work == NULL)
		return NULL;
	if (work->next == NULL) {
		work_queue->first = NULL;
		work_queue->last = NULL;
	} else {
		work_queue->first = work->next;
	}
	return work;
}

static void *t_worker(void *arg) 
{
	t_work_queue_t *work_queue = arg;
	t_work_t *work;
	while (1) {
		pthread_mutex_lock(&(work_queue->mtx));
		while (work_queue->first == NULL && !work_queue->stop) 
			pthread_cond_wait(&(work_queue->work_cond), &(work_queue->mtx));
		if (work_queue->stop)
			break;
		work = t_work_get(work_queue);
		work_queue->num_working++;
		pthread_mutex_unlock(&(work_queue->mtx));
		if (work != NULL) {
			work->func(work->arg);
			t_work_free(work);
		}
		pthread_mutex_lock(&(work_queue->mtx));
		work_queue->num_working--;
		if (!work_queue->stop && work_queue->num_working == 0 && work_queue->first == NULL) 
			pthread_cond_signal(&(work_queue->busy_cond));
		pthread_mutex_unlock(&(work_queue->mtx));
	}
	work_queue->num_active--;
	pthread_cond_signal(&(work_queue->busy_cond));
	pthread_mutex_unlock(&(work_queue->mtx));
	return NULL;
}

t_work_queue_t *tpool_create(void)
{
	t_work_queue_t *work_queue;
	pthread_t thread;
	int t;
	work_queue = calloc(1, sizeof(*work_queue));
	work_queue->num_active = NTHREADS;
	pthread_mutex_init(&(work_queue->mtx), NULL);
	pthread_cond_init(&(work_queue->work_cond), NULL);
	pthread_cond_init(&(work_queue->busy_cond), NULL);
	work_queue->first = NULL;
	work_queue->last = NULL;
	for (t = 0; t < NTHREADS; ++t) {
		pthread_create(&thread, NULL, t_worker, work_queue);
		pthread_detach(thread);
	}
	return work_queue;
}

bool t_work_new(t_work_queue_t *work_queue, thread_func_t func, void *arg)
{
	t_work_t *work;
	if (work_queue == NULL)
		return false;
	work = t_work_create(func, arg);
	if (work == NULL) return false;
	pthread_mutex_lock(&(work_queue->mtx));
	if (work_queue->first == NULL) {
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

void tpool_wait(t_work_queue_t *work_queue)
{
	if (work_queue == NULL) 
		return; 
	pthread_mutex_lock(&(work_queue->mtx));
	while (1) {
		if ((!work_queue->stop && work_queue->num_working != 0) || (work_queue->stop && work_queue->num_active != 0)) {
			pthread_cond_wait(&(work_queue->busy_cond), &(work_queue->mtx));
		} else {
			break; 
		}
	}
	pthread_mutex_unlock(&(work_queue->mtx));
}

void tpool_destroy(t_work_queue_t *work_queue)
{
	t_work_t *work, *work2;
	if (work_queue == NULL) 
		return;
	pthread_mutex_lock(&(work_queue->mtx));
	work = work_queue->first;
	while (work != NULL) {
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

sig_atomic_t keep_serving = 1;

void term_handler(int signum)
{
	keep_serving = 0;
}

void test_func(int *anything)
{
	printf("%p\n", pthread_self());	
}

void serve(void)
{
	/* Test thread pool */
	t_work_queue_t *work_queue = tpool_create();
	int anum = 5;
	int *anything = &anum;
	while (keep_serving) {
		t_work_new(work_queue, test_func, anything);
		t_work_new(work_queue, test_func, anything);
		t_work_new(work_queue, test_func, anything);
		t_work_new(work_queue, test_func, anything);
	}
	tpool_wait(work_queue);
	tpool_destroy(work_queue);
}

int main(void)
{
	serve();
	return 0;
}
