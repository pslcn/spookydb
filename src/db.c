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

sig_atomic_t keep_serving = 1;

void term_handler(int signum)
{
	keep_serving = 0;
}

void test_func(char *something)
{
	printf("%p\n", pthread_self());
}

int main(void)
{
	t_work_queue_t *work_queue;
	t_pool_create(&work_queue);
	while (keep_serving) {
		t_work_new(work_queue, test_func, "Anything!\n");
	}
	t_pool_wait(work_queue);
	t_pool_destroy(work_queue);
	return 0;
}
