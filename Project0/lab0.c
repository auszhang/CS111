/*
 * Name:	Austin Zhang
 * Email: 	auszhang@ucla.edu
 * ID:    	604736503
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>

#include <fcntl.h>
#include <unistd.h>

#define INPUT 'i'
#define OUTPUT 'o'
#define SEGFAULT 's'
#define CATCH 'c'

void print_error(char* arg, char* file, int errnum) {
    fprintf(stderr, "ERROR: The file \'%s\' could not be opened due to error: %s.\n Problem caused by argument \'--%s\'.\n", file, strerror(errnum), arg);
}

void create_segfault(){
    char * p = NULL;
    *p = 'p';
    return;
}

void handle_signal(int sig) {
    if(sig == SIGSEGV){
        fprintf(stderr, "Signal handler: Segmentation fault caught. Exiting now.\n");
        exit(4);
    }
}

void redirect_input( char* input_file) {
    int ifd = open(input_file, O_RDONLY);
    if (ifd >= 0) {
        if ( close(0) < 0 || dup(ifd) < 0 || close(ifd)) {
            fprintf(stderr, "Unable to open specified input file. Exiting now.\n");
            exit(2);
        }
    } else {
        print_error("input", input_file, errno);
        exit(2);
    } 
}

void redirect_output(char* output_file) {
    int ofd = creat(output_file, 0666);
    if (ofd >= 0) {
        if (close(1) < 0 || dup(ofd) < 0 || close(ofd)) {
            fprintf(stderr, "Unable to create specified output file. Exiting now.\n");
            exit(3);
        }
    } else {
        print_error("output", output_file, errno);
        exit(3);
    }
}

void copy_files() {
    char input;
    int re;
    // run read from FD0 until EOF/error and write to FD1
    while((re = read(0, &input, sizeof(char))) > 0) {
        if (write(1, &input, sizeof(char)) < 0) {
            fprintf(stderr, "Could not write to output file. %s\n", strerror(errno));
            exit(3);
        }
    }

    // exit(3) if an error was encountered
    if(re < 0) {
        fprintf(stderr, "asdfghjkl Could not read from input file. %s\n", strerror(errno));
        exit(3);
    }
}

int main(int argc, char * argv[]) {

    int ops;

    // flags
    bool input_flag = false;
    bool output_flag = false;
    bool segfault_flag = false;
    bool catch_flag = false;

    // files
    char * input_file = NULL;
    char * output_file = NULL;

    // possible options
    struct option long_options[] = { 
		{"input",    required_argument, NULL, INPUT},
		{"output",   required_argument, NULL, OUTPUT},
		{"segfault", no_argument,       NULL, SEGFAULT},
		{"catch",    no_argument,       NULL, CATCH},
		{0,          0,                 0,    0}
	};

    // parse command and get options
    while((ops = getopt_long(argc, argv, "", long_options, NULL)) != -1) {
        switch(ops) {
            case INPUT:
                input_flag = true;
                input_file = optarg;
                break;
            case OUTPUT:
                output_flag = true;
                output_file = optarg;
                break;
            case SEGFAULT:
                segfault_flag = true;
                break;
            case CATCH: 
                catch_flag = true;
                break;
            default:
                //invalid argument
                fprintf(stderr, "Unrecognized arguments: use lab0 [--input=filename] [--output=filename] [--segfault] [--catch]");
                exit(1);
        }
    }

    // handles flags in this order
    if(catch_flag)
        signal(SIGSEGV, handle_signal);
    if(segfault_flag)
        create_segfault();
    if(input_flag)
        redirect_input(input_file);
    if(output_flag)
        redirect_output(output_file);

    // copy input to output
    if(!segfault_flag)
    copy_files();
    exit(0);
}