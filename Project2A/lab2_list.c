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
#include "SortedList.h"

SortedList_t head;
SortedListElement_t* elements;

unsigned long threads;
long iterations;
int lock;
char sync_arg;
pthread_mutex_t mutex_lock;

void print_error (char * message) {
    fprintf(stderr, message);
    exit(1);
}

void handle_segfault() {
	fprintf(stderr, "Segmentation fault has reached the handler. Exiting.");
	exit(2);
}

void* thread_function(void *val) {
	SortedListElement_t* ar = val;

	// lock the mutex or test and set the lock
	switch(sync_arg) {
		case 'm':
			pthread_mutex_lock(&mutex_lock);
			break;
		case 's':
			while (__sync_lock_test_and_set(&lock, 1));
			break;
		default:
			break;
	}

	long i;

	for (i = 0; i < iterations; i++) {
		SortedList_insert(&head, (SortedListElement_t *) (ar+i));
	}

	long len = SortedList_length(&head);

	if (len < iterations) {
		fprintf(stderr, "Not all items inserted in list.\n");
		exit(2);
	}

	char *curr_key = malloc(sizeof(char)*256);
	SortedListElement_t *list_item;

	for (i = 0; i < iterations; i++) {
		strcpy(curr_key, (ar+i)->key);
    	if ((list_item = SortedList_lookup(&head, curr_key)) == NULL) {
    		fprintf(stderr, "Failed to find element. \n");
    		exit(2);
    	}
		if (SortedList_delete(list_item) != 0) {
			fprintf(stderr, "Failed to delete element.\n");
			exit(2);
		}
	}

   	// unlock the mutex or release the lock
	switch (sync_arg) {
		case 'm':
      		pthread_mutex_unlock(&mutex_lock);
      		break;		
      	case 's':
      		__sync_lock_release(&lock);
      		break;
      	default:
      		return NULL;
	}
	return NULL;
}

void parse_options(int argc, char *argv[]) {
	threads = 1;
	iterations = 1;
	opt_yield = 0;
	sync_arg = 0;

	struct option options[] = {
		{"threads", required_argument, NULL, 't'},
		{"iterations", required_argument, NULL, 'i'},
		{"yield", required_argument, NULL, 'y'},
		{"sync", required_argument, NULL, 's'},
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
				if (sync_arg == 'm' && pthread_mutex_init(&mutex_lock, NULL) != 0) {
			      	print_error( "Could not create mutex\n");
			  	}
				break;
			default:
				print_error("Invalid option: see usage.\n");
		}
	}
}

int main(int argc, char* argv[]) {
	unsigned long i;
	unsigned long j;

	parse_options(argc, argv);

	if(signal(SIGSEGV, handle_segfault) == SIG_ERR){
		print_error("Failed to set signal handler.");
	}

	unsigned long num_elements = threads * iterations;
	elements = malloc(sizeof(SortedListElement_t) * num_elements);

	char** keys = malloc(iterations * threads * sizeof(char*));

	for (i = 0; i < num_elements; i++) {
		keys[i] = malloc(sizeof(char) * 256);
		
		for (j = 0; j < 255; j++) {
			keys[i][j] = rand() % 94 + 33;
		}

		keys[i][255] = '\0';
		(elements + i)->key = keys[i];
	}

	pthread_t *tids = malloc(sizeof(pthread_t) * threads);

	// start timer
  	struct timespec start, end;
  	if (clock_gettime(CLOCK_MONOTONIC, &start) < 0) {
  		print_error("Failed to get start time. \n");
  	}

  	// create threads
  	for (i = 0; i < threads; i++) {
  		if (pthread_create(&tids[i], NULL, &thread_function, (void *) (elements + iterations * i)) != 0) {
  			print_error("Failed to create threads\n");
  		}
  	}

  	// join threads
  	for (i = 0; i < threads; i++) {
  		pthread_join(tids[i], NULL);
  	}

  	// stop timer
  	if (clock_gettime(CLOCK_MONOTONIC, &end) < 0) {
  		print_error("Failed to get end time. \n");
  	}

  	if (SortedList_length(&head) != 0) {
  		fprintf(stderr, "Length of list not 0 at end.\n");
  		exit(2);
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
	long num_lists = 1;

	fprintf(stdout, ",%ld,%ld,%ld,%ld,%ld,%ld\n", threads, iterations, num_lists, num_ops, run_time, avg_time);

	// clean up before successful exit
	
	if (sync_arg == 'm') {
		pthread_mutex_destroy(&mutex_lock);
	}
	free(elements);
	free(tids);
	exit(0);
}