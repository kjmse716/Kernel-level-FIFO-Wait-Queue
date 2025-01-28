#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/syscall.h>
#define NUM_THREADS 10

#define SYS_CALL_MY_WAIT_QUEUE 452

void *enter_wait_queue(void *thread_id)
{
	pid_t tid = syscall(SYS_gettid); // get thread id
	fprintf(stderr, "enter wait queue thread_id: %d, current thread id = %d\n", *(int *)thread_id, tid);	

	syscall(SYS_CALL_MY_WAIT_QUEUE, 1);

	fprintf(stderr, "exit wait queue thread_id: %d, current thread id = %d\n", *(int *)thread_id, tid);
}
void *clean_wait_queue()
{

	syscall(SYS_CALL_MY_WAIT_QUEUE, 2);
}

int main()
{
	void *ret;
	pthread_t id[NUM_THREADS];
	int thread_args[NUM_THREADS];

	for (int i = 0; i < NUM_THREADS; i++)
 	{
 		thread_args[i] = i;
 		pthread_create(&id[i], NULL, enter_wait_queue, (void *)&thread_args[i]);
 	}
 	sleep(1);
 	
//	fprintf(stderr, "start clean queue ...\n");
//      fprintf(stderr, "start clean queue, executed by PID: %ld\n", syscall(SYS_gettid));
	
	
	clean_wait_queue();
 	
	for (int i = 0; i < NUM_THREADS; i++)
 	{
 		pthread_join(id[i], &ret);
 	}
	 return 0;
}

