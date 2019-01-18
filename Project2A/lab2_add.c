/*
 * NAME: Austin Zhang
 * EMAIL: aus.zhang@gmail.com
 * ID: 604736503
 */

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <sched.h>

long threads;
long iterations;
long long count;
char sync_arg;
int yield_flag;

int lock = 0;
pthread_mutex_t mutex_lock;
pthread_t *tids;

void print_error (char * message) {
    fprintf(stderr, message);
    exit(1);
}

void add_mutex(long long *pointer, long long value) {
 	pthread_mutex_lock(&mutex_lock);
  	long long sum = *pointer + value;
  	if (yield_flag) {
   		sched_yield();
	}
 	*pointer = sum;
  	pthread_mutex_unlock(&mutex_lock);
}

void add_spinlock(long long *pointer, long long value) {
	while (__sync_lock_test_and_set(&lock, 1));
	long long sum = *pointer + value;
	if (yield_flag) {
    		sched_yield();
    	}
  	*pointer = sum;
 	 __sync_lock_release(&lock);
}

void add_compareswap(long long *pointer, long long value) {
	long long ex;
	do {
		ex = count;
	    	if (yield_flag) {
	     		sched_yield();
	    	}
  	} while (__sync_val_compare_and_swap(pointer, ex, ex+value) != ex);
}

void add_default(long long *pointer, long long value) {
	long long sum = *pointer + value;
	if (yield_flag) {
    	sched_yield();
	}
   	*pointer = sum;
}

void* thread_add(void* arg) {
	long i;	
	for (i = 0; i < iterations; i++) {
		switch(sync_arg) {
			case 'm': 
				add_mutex(&count, 1);
				add_mutex(&count, -1);
			case 's':
				add_spinlock(&count, 1);
				add_spinlock(&count, -1);
			case 'c':
				add_compareswap(&count, 1);
				add_compareswap(&count, -1);
			default:
				add_default(&count, 1);
				add_default(&count, -1);		
		}
	}
	return arg;
}

void parse_options(int argc, char *argv[]) {
	threads = 1;
	iterations = 1;
	yield_flag = 0;
	sync_arg = 0;

	struct option options[] = {
		{"threads", required_argument, NULL, 't'},
		{"iterations", required_argument, NULL, 'i'},
		{"yield", no_argument, NULL, 'y'},
		{"sync", required_argument, NULL, 's'},
		{0, 0, 0, 0}
	};

	int long_option;
	while ((long_option = getopt_long(argc, argv, "", options, NULL)) != -1) {
		switch (long_option) {
			case 't':
				threads = atoi(optarg);
				break;
			case 'i':
				iterations = atoi(optarg);
				break;
			case 'y':
				yield_flag = 1;
				break;
			case 's':
				sync_arg = optarg[0];
				break;
			default:
				print_error("Invalid option: see usage.\n");
		}
	}
}

void start_threads() {
	int i;

	tids = malloc(threads * sizeof(pthread_t));

	if (tids == NULL) {
		print_error("Failed to allocate memory for threads.\n");
	}
	
	// create threads
	for (i = 0; i < threads; i++) {
		if (pthread_create(&tids[i], NULL, &thread_add, NULL) != 0) {
			print_error("Could not create threads.\n");
		}
	}
	
	// join threads
	for (i = 0; i < threads; i++) {
		if(pthread_join(tids[i], NULL) != 0) {
			free(tids);
			print_error("Failed to join threads.\n");
		}
	}
}

int main(int argc, char* argv[]) {
	parse_options(argc, argv);

	count = 0;

	// start timer
	struct timespec start, end;
	if (clock_gettime(CLOCK_MONOTONIC, &start) < 0) {
		print_error("Failed to fetch start time.\n");
	}

	if (sync_arg == 'm') {
		if (pthread_mutex_init(&mutex_lock, NULL) != 0){
			print_error("Failed to create mutex.\n");
		}	
	}

	start_threads();

	// stop timer
	if (clock_gettime(CLOCK_MONOTONIC, &end) < 0) {
		print_error("Failed to fetch end time.\n");
	}

	long num_ops = threads * iterations * 2;
	long run_time = 1000000000L * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
	long avg_time = run_time / num_ops;

	// print data
	switch(yield_flag) {
		case 0:
			if (sync_arg == 'm')
				fprintf(stdout, "add-m,%ld,%ld,%ld,%ld,%ld,%lld\n", threads, iterations, num_ops, run_time, avg_time, count);
			else if (sync_arg == 'c')
				fprintf(stdout, "add-c,%ld,%ld,%ld,%ld,%ld,%lld\n", threads, iterations, num_ops, run_time, avg_time, count);
			else if (sync_arg == 's')
	    		fprintf(stdout, "add-s,%ld,%ld,%ld,%ld,%ld,%lld\n", threads, iterations, num_ops, run_time, avg_time, count);
	    	else
	    		fprintf(stdout, "add-none,%ld,%ld,%ld,%ld,%ld,%lld\n", threads, iterations, num_ops, run_time, avg_time, count);
		case 1: 
			if (sync_arg == 'm')
				fprintf(stdout, "add-yield-m,%ld,%ld,%ld,%ld,%ld,%lld\n", threads, iterations, num_ops, run_time, avg_time, count);
			else if (sync_arg == 's')
	    		fprintf(stdout, "add-yield-s,%ld,%ld,%ld,%ld,%ld,%lld\n", threads, iterations, num_ops, run_time, avg_time, count);
			else if (sync_arg == 'c')
	    		fprintf(stdout, "add-yield-c,%ld,%ld,%ld,%ld,%ld,%lld\n", threads, iterations, num_ops, run_time, avg_time, count);
			else if (sync_arg == 0)
			    fprintf(stdout, "add-yield-none,%ld,%ld,%ld,%ld,%ld,%lld\n", threads, iterations, num_ops, run_time, avg_time, count);
			else 
				fprintf(stdout, "add-none,%ld,%ld,%ld,%ld,%ld,%lld\n", threads, iterations, num_ops, run_time, avg_time, count);
		default: 
	    	break;
	}

	if (sync_arg == 'm')
	    pthread_mutex_destroy(&mutex_lock);

	free(tids);
	exit(0);
}