/* 
 * NAME: Austin Zhang
 * EMAIL: aus.zhang@gmail.com
 * ID: 604736503
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <poll.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <netdb.h>
#include <netinet/in.h>
#include <mcrypt.h> //  for encryption 
#include <string.h>


#define CARRIAGE_RETURN '\015' //carriage return
#define LINE_FEED '\012' //line feed
#define ESCAPE '\004' //^D escape sequence
#define TEXT_END '\003' //^C end of text

struct termios old_mode;
char crlf[2] = {CARRIAGE_RETURN, LINE_FEED};
char lf[1] = {LINE_FEED};

int pid;
int to_child_pipe[2];
int from_child_pipe[2];
int key_fd;
int size;
char * key;
char * IV;
int sock_fd;
MCRYPT encrypt_fd, decrypt_fd;

int port_flag = 0;
int log_flag = 0;
int encrypt_flag = 0;
char * encrypt_file;
int portnum = 0;

void print_shell_exit() {
	int status = 0;
    waitpid(0, &status, 0);
    fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WTERMSIG(status), WEXITSTATUS(status));
}

void print_error (char * message) {
    fprintf(stderr, message);
    exit(1);
}

void parse_options(int argc, char * argv[]) {
    int ops;
    struct option long_options[] = { 
		{"port",    required_argument, NULL, 'p'},
        {"encrypt", required_argument, NULL, 'e'},
		{0,         0,                 0,    0}
	};
    while((ops = getopt_long(argc, argv, "", long_options, NULL)) != -1) {
        switch(ops) {
            case 'p':
                port_flag = 1;
                portnum = atoi(optarg); //converts optarg string to int
                break;
            case 'e':
                encrypt_flag = 1;
                encrypt_file = optarg;
                break;
            default:
                print_error("Unrecognized arguments: use lab1b --port=port# [--encrypt=filename]\n");
        }
    }
}

void handle_sigpipe() {
    close(to_child_pipe[1]);
    close(from_child_pipe[0]);
    kill(pid, SIGINT);
    print_shell_exit();
    exit(0);
}

void set_up_encryption() {
	int i;

    if((key_fd = open(encrypt_file, O_RDONLY)) < 0) {
        print_error("Failed to open key file descriptor during encryption.");
    }

    struct stat s;
    if(fstat(key_fd, &s) < 0) {
        close(key_fd);
        print_error("Failed to return key information.");
    }
    size = (int) s.st_size;
    key = (char*) malloc(size * sizeof(char));
    read(key_fd, key, size);
    close(key_fd);

    // initialize encryption module
    if((encrypt_fd = mcrypt_module_open("twofish", NULL, "cfb", NULL)) == MCRYPT_FAILED) {
        print_error("Failed to open mcrypt library module for encryption.");
    }
    IV = malloc(mcrypt_enc_get_iv_size(encrypt_fd));
    for(i = 0 ; i < mcrypt_enc_get_iv_size(encrypt_fd); i++ ) {
        IV[i] = rand();
    }
    if(mcrypt_generic_init(encrypt_fd, key, 16, IV) < 0) {
        print_error("Failed to initialize encryption module.");
    }

    // initalize decryption module
    if((decrypt_fd = mcrypt_module_open("twofish", NULL, "cfb", NULL)) == MCRYPT_FAILED) {
        print_error("Failed to open mcrypt library module for decryption.");
    }
    if(mcrypt_generic_init(decrypt_fd, key, 16, IV) < 0) {
        print_error("Failed to initalize decryption module.");
    }
    free(IV);
    free(key);
}

int main(int argc, char * argv[]) {

    /*** Parse options ***/
    parse_options(argc, argv);
    
    if(!port_flag){
        print_error("Error: port option is mandatory. Use '--port=#'");
    }
    if(encrypt_flag){
        set_up_encryption();
    }
    
    /*** Create a socket @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ ***/
    // might need more variables?
    int new_sock_fd;
    socklen_t client_size;
    struct sockaddr_in server_address, client_address;

    // call socket()
    if((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        print_error("Client: failed to create socket.");
    }

    // initialize socket structure
    memset((char *)&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(portnum);

    // bind host address
    if(bind(sock_fd, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
    	print_error("Failed to assign address to server.");
    }

    // listen for clients 
    if(listen(sock_fd, 5)) {
    	print_error("Failed to mark socket to accept incoming connection.");
    }

    // accept connection from client
    client_size = sizeof(client_address);
    if((new_sock_fd = accept(sock_fd, (struct sockaddr *) &client_address, &client_size)) < 0) {
    	close(new_sock_fd);
	    close(sock_fd);
    	print_error("Failed to accept connection from client.");
    }

    // start communicating
	if (pipe(to_child_pipe) < 0 || pipe(from_child_pipe) < 0 ) {
		close(new_sock_fd);
	    close(sock_fd);
        print_error("Failed to create pipe.");
    }

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
	    char * args = "/bin/bash";
	    char * exec_args[2] = {args, NULL};
	    if(execvp(args, exec_args) < 0) {
	    	if(encrypt_flag) {
		        mcrypt_generic_deinit(encrypt_fd);
		        mcrypt_module_close(encrypt_fd);
		        mcrypt_generic_deinit(decrypt_fd);
		        mcrypt_module_close(decrypt_fd);
		    }
		    print_shell_exit();
	    	close(new_sock_fd);
	    	close(sock_fd);
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
	        {new_sock_fd, POLLIN, 0}, // descriptor for keyboard (stdin)
	        {from_child_pipe[0], POLLIN, 0} // descriptor for pipe that returns output from shell
	    };

	    int status;
	    int i;
	    const int bufsize = 256;

	    while(1) {
	        int poll_result = poll(pollfd_descriptors, 2, 0);
	        if(poll_result < 0) {
	        	print_shell_exit();
	        	close(new_sock_fd);
	        	close(sock_fd);
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
	            int readin = read(new_sock_fd, read_buff, bufsize);
	            if(readin < 0) {
	            	print_error("Error in read.");
	            } else if (readin == 0) {
	            	kill(0, SIGTERM);
	            }
	            for(i = 0; i < readin; i++) {
	            	if(encrypt_flag){
	            		mdecrypt_generic(decrypt_fd, read_buff + i, 1);
	            	}
	            	if(read_buff[i] == CARRIAGE_RETURN || read_buff[i] == LINE_FEED) {
	                    char lf = LINE_FEED;
	                    if(write(to_child_pipe[1], &lf, 1) < 0) {
	                        print_error("Failed to write to terminal. [2]");
	                    } 
	                }
	                else if(read_buff[i] == ESCAPE) {
	                    close(to_child_pipe[1]);
	                } else if(read_buff[i] == TEXT_END) {
	                    if(kill(pid, SIGINT) < 0) {
	                        print_error("Failed to kill child process.");
	                    }
	                } else {
					    if(write(to_child_pipe[1], read_buff + i, 1) < 0) {
	                        print_error("Failed to write to terminal. [4]");
					    }
	                } 
	            }
	        } else if(terminal_poll == POLLERR) {
	            print_error("Failed to poll terminal.");
	        }

	        // check shell readin
	        if(shell_poll == POLLIN) {
	            char read_buff[bufsize];
	            int readin = read(from_child_pipe[0], &read_buff, bufsize);
	            for(i = 0; i < readin; i++){
	                if(encrypt_flag) {
	                	mcrypt_generic(encrypt_fd, read_buff+i, 1);
	                }
	            	if(write(new_sock_fd, read_buff+i, 1) < 0) {
	                	print_error("Failed to write. [3]");
	            	}
	        	}

	        } else if(shell_poll & POLLERR || shell_poll & POLLHUP){
	            close(from_child_pipe[0]);
	            if(encrypt_flag) {
			        mcrypt_generic_deinit(encrypt_fd);
			        mcrypt_module_close(encrypt_fd);
			        mcrypt_generic_deinit(decrypt_fd);
			        mcrypt_module_close(decrypt_fd);
			    }
			    print_shell_exit();
	            close(new_sock_fd);
	            close(sock_fd);
	            exit(0);
	        }
	    }

	    //close pipes
	    if(close(from_child_pipe[0]) < 0)
	        print_error("Failed to close pipe from child to parent.");
	    if(close(to_child_pipe[1]) < 0) 
	        print_error("Failed to close pipe from parent to child.");
	    if(close(sock_fd) < 0) 
	        print_error("Failed to close original socket.");
	    if(close(new_sock_fd) < 0) 
	        print_error("Failed to close new socket.");

	    waitpid(pid, &status, 0);
	    fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WIFSIGNALED(status), WEXITSTATUS(status));
	    exit(0);

	} else {
	    // fork failed 
	    if(encrypt_flag) {
	        mcrypt_generic_deinit(encrypt_fd);
	        mcrypt_module_close(encrypt_fd);
	        mcrypt_generic_deinit(decrypt_fd);
	        mcrypt_module_close(decrypt_fd);
	    }
	    print_shell_exit();
	    close(new_sock_fd);
	    close(sock_fd);
	    print_error("Failed to fork process.");
	} 

    // prepare for exit
    close(new_sock_fd);
	close(sock_fd);
    if(encrypt_flag) {
        mcrypt_generic_deinit(encrypt_fd);
        mcrypt_module_close(encrypt_fd);
        mcrypt_generic_deinit(decrypt_fd);
        mcrypt_module_close(decrypt_fd);
    }
    print_shell_exit();
    exit(0);
}