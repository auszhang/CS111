/*
 * NAME:	Austin Zhang
 * EMAIL: 	aus.zhang@gmail.com
 * ID:    	604736503
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <poll.h>
#include <sys/wait.h>

#define CARRIAGE_RETURN '\015' //carriage return
#define LINE_FEED '\012' //line feed
#define ESCAPE '\004' //^D escape sequence
#define TEXT_END '\003' //^C end of text

struct termios old_mode;
char crlf[2] = {CARRIAGE_RETURN, LINE_FEED};

int pid;
int to_child_pipe[2];
int from_child_pipe[2];

int shell_flag=0;
char * shellarg = NULL;

void print_error (char * message) {
    fprintf(stderr, message);
    exit(1);
}

void parse_options(int argc, char * argv[]) {
    int ops;
    struct option long_options[] = { 
		{"shell",    required_argument, NULL, 's'},
		{0,          0,                 0,    0}
	};
    while((ops = getopt_long(argc, argv, "", long_options, NULL)) != -1) {
        switch(ops) {
            case 's':
                shell_flag = 1;
                shellarg = optarg;
                break;
            default:
                print_error("Unrecognized arguments: use lab1a [--shell=filename]\n");
        }
    }
}

void restore_terminal() {
    tcsetattr(STDIN_FILENO, TCSANOW, &old_mode);
}

void set_terminal_mode() {
    /* Changes the terminal mode to accept non-canonical input with no echo */
    
    // save old terminal 
    tcgetattr(STDIN_FILENO, &old_mode);
    
    // restore old terminal on exit
    atexit(restore_terminal);

    // make a copy of the terminal, from the spec...
    struct termios new_mode;
    tcgetattr(STDIN_FILENO, &new_mode);
    new_mode.c_iflag = ISTRIP;	/* only lower 7 bits	*/
	new_mode.c_oflag = 0;		/* no processing	*/
	new_mode.c_lflag = 0;		/* no processing	*/
    tcsetattr(STDIN_FILENO, TCSANOW, &new_mode);
}

void handle_sigpipe() {
    int status;
    close(to_child_pipe[1]);
    close(from_child_pipe[0]);
    kill(pid, SIGINT);
    waitpid(pid, &status, 0);
    fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WIFSIGNALED(status), WEXITSTATUS(status));
    exit(0);
}

int main(int argc, char * argv[]) {

    /*** Parse options ***/
    parse_options(argc, argv);
    
    
    /*** Handles shell flag ***/
    if(shell_flag) {
        //create pipes
        if (pipe(to_child_pipe) < 0 || pipe(from_child_pipe) < 0 ) {
            print_error("Failed to create pipe.");
        }

        set_terminal_mode();
        signal(SIGPIPE, handle_sigpipe);

        pid = fork();
        
        if(pid == 0) {
            /* shell process */
            // close unused pipes
            if( close(to_child_pipe[1]) < 0 || 
                close(from_child_pipe[0]) < 0) {
                print_error("Failed to close unused pipes in shell process.");
            } 

            // replace STDIN, STDOUT, and STDERR with pipes
            if( dup2(to_child_pipe[0], STDIN_FILENO) < 0 ||
                dup2(from_child_pipe[1], STDOUT_FILENO) < 0 ||
                dup2(from_child_pipe[1], STDERR_FILENO) < 0) {
                    print_error("Failed to redirect pipes.");
            }

            // close original pipes
            if( close(to_child_pipe[0]) < 0 ||
                close(from_child_pipe[1]) < 0) {
                    print_error("Failed to close original pipes.");
                }

            // start shell (the shell will read from the pipe and execute)
            char * args = shellarg;
            char * exec_args[2] = {args, NULL};
            if(execvp(args, exec_args) < 0) {
                print_error("Failed to start shell process.");
            }
        } else if(pid > 0) {
            /* terminal process */
            //close unused pipes
            if( close(from_child_pipe[1]) < 0 ||
                close(to_child_pipe[0]) < 0) {
                    print_error("Failed to close unused pipes in terminal process.");
                }

            // create array of two pollfd structures
            struct pollfd pollfd_descriptors[] = {
                {STDIN_FILENO, POLLIN, 0}, // descriptor for keyboard (stdin)
                {from_child_pipe[0], POLLIN, 0} // descriptor for pipe that returns output from shell
            };

            int status;
            int i;
            const int bufsize = 256;

            int end = 0;

            while(end == 0) {
                int poll_result = poll(pollfd_descriptors, 2, 0);
                if(poll_result < 0) {
                    print_error("Failed to poll.");
                }
                if(poll_result == 0)
                    continue;
                
                // check revents from pollfd
                short terminal_poll = pollfd_descriptors[0].revents;
                short shell_poll = pollfd_descriptors[1].revents;

                // check terminal readin
                if(terminal_poll == POLLIN) {
                    char read_buff[bufsize];
                    int readin = read(STDIN_FILENO, &read_buff, bufsize);
                    for(i = 0; i < readin; i++) {
                        if(read_buff[i] == TEXT_END) {
                            if(kill(pid, SIGINT) < 0) {
                                print_error("Failed to kill child process.");
                            }
                        } else if(read_buff[i] == ESCAPE) {
                            end = 1;
                        } else if(read_buff[i] == CARRIAGE_RETURN || read_buff[i] == LINE_FEED) {
                            char lf = LINE_FEED;
                            if(write(STDOUT_FILENO, crlf, 2) < 0) 
                                print_error("Failed to write to terminal. [1]");
                            if(write(to_child_pipe[1], &lf, 1) < 0)
                                print_error("Failed to write to terminal. [2]");
                        } else {
                            if(write(STDOUT_FILENO, (read_buff + i), 1) < 0) 
                                print_error("Failed to write to terminal. [3]");
						    if(write(to_child_pipe[1], (read_buff + i), 1) < 0) 
                                print_error("Failed to write to terminal. [4]");
                        }   
                    }
                } else if(terminal_poll == POLLERR) {
                    print_error("Failed to poll terminal.");
                }

                // check shell readin
                if(shell_poll == POLLIN) {
                    char read_buff[bufsize];
                    int readin = read(from_child_pipe[0], &read_buff, bufsize);
                    int j;
                    int count = 0;
                    for(i = 0, j = 0; i < readin; i++){
                        if(read_buff[i] == ESCAPE) {
                            end = 1;
                        } else if(read_buff[i] == LINE_FEED) {
                            if(write(STDOUT_FILENO, (read_buff + j), count) < 0) 
                                print_error("Failed to write to shell. [1]");
							if(write(STDOUT_FILENO, crlf, 2) < 0)
                                print_error("Failed to write to shell. [2]");
							j += count + 1;
							count = 0;
							continue;
                        }
                        count++;
                    }
                    if(write(STDOUT_FILENO, (read_buff+j), count) < 0) 
                        print_error("Failed to write. [3]");

                } else if(shell_poll & POLLERR || shell_poll & POLLHUP){
                    end = 1;
                }
            }

            //close pipes
            if(close(from_child_pipe[0]) < 0)
                print_error("Failed to close pipe from child to parent.");
            if(close(to_child_pipe[1]) < 0) 
                print_error("Failed to close pipe from parent to child.");

            waitpid(pid, &status, 0);
            fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WIFSIGNALED(status), WEXITSTATUS(status));
            exit(0);

        } else {
            // fork failed 
            print_error("Failed to fork process.");
        } 
        
    }

    /*** Handle no shell option ***/
    set_terminal_mode();

    //read ASCII input from keyboard into buffer
    char input;
    while(read(STDIN_FILENO, &input, 10) > 0 && input != ESCAPE) {
        if (input == CARRIAGE_RETURN || input == LINE_FEED) {
            input = CARRIAGE_RETURN;
            write(STDOUT_FILENO, &input, 1);
            input = LINE_FEED;
            write(STDOUT_FILENO, &input, 1);
        } else {
            write(STDOUT_FILENO, &input, 1);
        }
    }    
    exit(0);
}