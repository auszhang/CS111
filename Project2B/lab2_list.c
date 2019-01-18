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
#include <limits.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include "SortedList.h"

SortedList_t* head;
SortedListElement_t* elements;

unsigned long lists;
unsigned long threads;
unsigned long iterations;
int * lock;
char sync_arg;
pthread_mutex_t * mutex_lock;

unsigned int hash(const char* key) {
	int ret = 31;

 	while (*key) {
		ret = 28 * ret ^ *key;
		key++;
  	}
  	return ret % lists;
}

void print_error (char * message) {
    fprintf(stderr, message);
    exit(1);
}

void handle_segfault() {
	fprintf(stderr, "Segmentation fault has reached the handler. Exiting.");
	exit(2);
}

void* thread_function(void *val) {
	unsigned long i;
	struct timespec start, end;
	long time = 0;
	SortedListElement_t* ar = val;

	for (i = 0; i < iterations; i++) {
		unsigned int h = hash((ar + i)->key);
		clock_gettime(CLOCK_MONOTONIC, &start);
		switch(sync_arg) {
			case 'm':
				pthread_mutex_lock(mutex_lock + h);
				break;
			case 's':
				while (__sync_lock_test_and_set(lock + h, 1));
				break;
			default:
				break;
		}
		clock_gettime(CLOCK_MONOTONIC, &end);
		time += 1000000000L * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
		SortedList_insert(head + h, (SortedListElement_t *) (ar+i));
		
		// unlock the mutex or release the lock
		switch (sync_arg) {
			case 'm':
	      		pthread_mutex_unlock(mutex_lock + h);
	      		break;		
	      	case 's':
	      		__sync_lock_release(lock + h);
	      		break;
	      	default:
	      		break;
		}
	}

	// total list length
	unsigned long list_len = 0;
	for (i = 0; i < lists; i++) {
		switch (sync_arg) {
			case 'm':
				clock_gettime(CLOCK_MONOTONIC, &start);
	      		pthread_mutex_lock(mutex_lock + i);
	      		clock_gettime(CLOCK_MONOTONIC, &end);
	      		time += 1000000000L * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
	      		break;		
	      	case 's':
	      		while (__sync_lock_test_and_set(lock + i, 1));
	      		break;
	      	default:
	      		break;
		}
	}

  	for (i = 0; i < lists; i++) {
    	list_len += SortedList_length(head + i);
    }

	if (list_len < iterations) {
		fprintf(stderr, "Not all items inserted in list.\n");
		exit(2);
	}

	for (i = 0; i < lists; i++) {
		switch (sync_arg) {
			case 'm':
	      		pthread_mutex_unlock(mutex_lock + i);
	      		break;		
	      	case 's':
	      		__sync_lock_release(lock + i);
	      		break;
	      	default:
	      		break;
	      	}
   	}

	char *curr_key = malloc(sizeof(char)*256);
	SortedListElement_t *list_item;

	for (i = 0; i < iterations; i++) {
		unsigned int h = hash((ar+i)->key);
		strcpy(curr_key, (ar+i)->key);
		clock_gettime(CLOCK_MONOTONIC, &start);
		switch(sync_arg) {
			case 'm':
				pthread_mutex_lock(mutex_lock + h);
				break;
			case 's':
				while (__sync_lock_test_and_set(lock + h, 1));
				break;
			default:
				break;
		}

		clock_gettime(CLOCK_MONOTONIC, &end);
		time += 1000000000L * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;

    	if ((list_item = SortedList_lookup(head + h, curr_key)) == NULL) {
    		fprintf(stderr, "Failed to find element. \n");
    		exit(2);
    	}
		if (SortedList_delete(list_item) != 0) {
			fprintf(stderr, "Failed to delete element.\n");
			exit(2);
		}

		// unlock the mutex or release the lock
		switch (sync_arg) {
		case 'm':
      		pthread_mutex_unlock(mutex_lock + h);
      		break;		
      	case 's':
      		__sync_lock_release(lock + h);
      		break;
      	default:
      		break;
		}
	}

	

	return (void *) time;
}

void parse_options(int argc, char *argv[]) {
	lists = 1;
	threads = 1;
	iterations = 1;
	opt_yield = 0;
	sync_arg = 0;


	struct option options[] = {
		{"threads", required_argument, NULL, 't'},
		{"iterations", required_argument, NULL, 'i'},
		{"yield", required_argument, NULL, 'y'},
		{"sync", required_argument, NULL, 's'},
		{"lists", required_argument, NULL, 'l'},
		{0, 0, 0, 0}
	};

	int opt;
	unsigned long i;
	while ((opt = getopt_long(argc, argv, "", options, NULL)) != -1) {
		switch (opt) {
			case 't':
				threads = atoi(optarg);
				break;
			case 'i':
				iterations = atoi(optarg);
				break;
			case 'y':
				for (i = 0; i < strlen(optarg); i++) {
					if (optarg[i] == 'i') {
						opt_yield |= INSERT_YIELD;
					} else if (optarg[i] == 'd') {
						opt_yield |= DELETE_YIELD;
					} else if (optarg[i] == 'l') {
						opt_yield |= LOOKUP_YIELD;
					}
				}
				break;
			case 's':
				sync_arg = optarg[0];
				break;
			case 'l':
				lists = atoi(optarg);
				break;
			default:
				print_error("Invalid option: see usage.\n");
				exit(1);
		}
	}
}

int main(int argc, char* argv[]) {
	unsigned long i, j;

	parse_options(argc, argv);
 
	if (signal(SIGSEGV, handle_segfault) == SIG_ERR){
		print_error("Failed to set signal handler.");
	}

	if ((head = malloc(sizeof(SortedList_t) * lists)) == NULL) {
		print_error("Failed to allocate memory to the start list");
	}

	// initialize list
	for (i = 0; i < lists; i++) {
		head[i].next = NULL;
		head[i].prev = NULL;
	}

	unsigned long num_elements = threads * iterations;
	elements = malloc(sizeof(SortedListElement_t) * num_elements);

	if (elements == NULL) {
		fprintf(stderr, "Failed to allocate memory for elements.\n");
		exit(1);
	}

	char** keys = malloc(iterations * threads * sizeof(char*));

	if (keys == NULL) {
		fprintf(stderr, "Failed to allocate memory for keys.\n");
		exit(1);
	}

	for (i = 0; i < num_elements; i++) {
		keys[i] = malloc(sizeof(char) * 256);
		
		for (j = 0; j < 255; j++) {
			keys[i][j] = rand() % 94 + 33;
		}

		keys[i][255] = '\0';
		(elements + i)->key = keys[i];
	}

	if (sync_arg == 'm') {
		if ((mutex_lock = malloc(sizeof(pthread_mutex_t) * lists)) == NULL) {
			print_error("Failed to allocate memory for mutex.");
		}
		for (i = 0; i < lists; i++) {
			if (pthread_mutex_init((mutex_lock + i), NULL) != 0) {
				print_error("Failed to create mutexes.");
			}
		}
	} else if (sync_arg == 's') {
		if((lock = malloc(sizeof(int) * lists)) == NULL) {
			print_error("Failed to allocate memory for spin lock.");
		}
		for (i = 0; i < lists; i++) {
			lock[i] = 0;
		}

	}

	pthread_t *tids = malloc(sizeof(pthread_t) * threads);

	// start timer
  	struct timespec start, end;
  	clock_gettime(CLOCK_MONOTONIC, &start);

  	// create threads
  	for (i = 0; i < threads; i++) {
  		if (pthread_create(&tids[i], NULL, &thread_function, (void *) (elements + iterations * i)) != 0) {
  			print_error("Failed to create threads\n");
  		}
  	}

  	long total = 0;
  	void ** pause = (void *) malloc(sizeof(void**));

  	// join threads
  	for (i = 0; i < threads; i++) {
  		pthread_join(tids[i], pause);
  		total += (long) *pause;
  	}

  	// stop timer
  	clock_gettime(CLOCK_MONOTONIC, &end);

  	long list_len = 0;
  	for (i = 0; i < lists; i++) {
  		list_len += SortedList_length(head + i);
  	}

  	fprintf(stdout, "list");

	switch(opt_yield) {
    	case 0:
			fprintf(stdout, "-none");
			break;
    	case 1:
			fprintf(stdout, "-i");
			break;
	   	case 2:
			fprintf(stdout, "-d");
			break;
    	case 3:
			fprintf(stdout, "-id");
			break;
    	case 4:
			fprintf(stdout, "-l");
			break;
    	case 5:
			fprintf(stdout, "-il");
			break;
    	case 6:
			fprintf(stdout, "-dl");
			break;
    	case 7:
			fprintf(stdout, "-idl");
			break;
    	default:
        	break;
	}

	switch(sync_arg) {
    	case 0:
        	fprintf(stdout, "-none");
        	break;
    	case 's':
        	fprintf(stdout, "-s");
        	break;
    	case 'm':
        	fprintf(stdout, "-m");
        	break;
    	default:
        	break;
	}
	  
	long num_ops = threads * iterations * 3;
	long run_time = 1000000000L * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
	long avg_time = run_time / num_ops;
	long num_lists = lists;
	long spin_time = total/num_ops;

	fprintf(stdout, ",%ld,%ld,%ld,%ld,%ld,%ld,%ld\n", threads, iterations, num_lists, num_ops, run_time, avg_time, spin_time);

	// clean up before successful exit
	
	if(sync_arg == 'm') {
		for (i = 0; i < lists; i++) {
			pthread_mutex_destroy(mutex_lock + i);
		}
		free(mutex_lock);
	} else if (sync_arg == 's') {
		free(lock);
	}

	free(elements);
	free(tids);
	free(pause);
	exit(0);
}