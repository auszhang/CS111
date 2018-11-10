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
#include <mcrypt.h>
#include <string.h>

#define CARRIAGE_RETURN '\015' //carriage return
#define LINE_FEED '\012' //line feed
#define ESCAPE '\004' //^D escape sequence
#define TEXT_END '\003' //^C end of text
#define SENT  14
#define RECEIVED 18

struct termios old_mode;
char crlf[2] = {CARRIAGE_RETURN, LINE_FEED};

int pid;
int to_child_pipe[2];
int from_child_pipe[2];
int log_fd = -1;
int key_fd;
int size;
char * key;
char * IV;
int sock_ret;
MCRYPT encrypt_fd, decrypt_fd;

int port_flag = 0;
int log_flag = 0;
int encrypt_flag = 0;
char * encrypt_file;
int portnum = 0;

void print_error (char * message) {
    fprintf(stderr, message);
    exit(1);
}

void parse_options(int argc, char * argv[]) {
    int ops;
    struct option long_options[] = { 
        {"port",    required_argument, NULL, 'p'},
        {"log",     required_argument, NULL, 'l'},
        {"encrypt", required_argument, NULL, 'e'},
        {0,         0,                 0,    0}
    };
    while((ops = getopt_long(argc, argv, "", long_options, NULL)) != -1) {
        switch(ops) {
            case 'p':
                port_flag = 1;
                portnum = atoi(optarg); //converts optarg string to int
                break;
            case 'l':
                log_flag = 1;
                if ((log_fd = creat(optarg, S_IRWXU)) < 0) {
                    print_error("Failed to create log file.");

                }
                break;
            case 'e':
                encrypt_flag = 1;
                encrypt_file = optarg;
                break;
            default:
                print_error("Unrecognized arguments: use lab1b --port=port# [--log=filename] [--encrypt=filename]\n");
        }
    }
}

void restore_terminal() {
    tcsetattr(STDIN_FILENO, TCSANOW, &old_mode);
    if(encrypt_flag) {
        mcrypt_generic_deinit(encrypt_fd);
        mcrypt_module_close(encrypt_fd);
        mcrypt_generic_deinit(decrypt_fd);
        mcrypt_module_close(decrypt_fd);
    }
    if(log_flag) {
        close(log_fd);
    }
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
    new_mode.c_iflag = ISTRIP;  /* only lower 7 bits    */
    new_mode.c_oflag = 0;       /* no processing    */
    new_mode.c_lflag = 0;       /* no processing    */
    tcsetattr(STDIN_FILENO, TCSANOW, &new_mode);
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
        print_error("Failed to open encryption module.");
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

    int i;
    const int bufsize = 256;
    /*** Parse options ***/
    parse_options(argc, argv);
    
    if(!port_flag){
        print_error("Error: port option is mandatory. Use '--port=#'");
    }
    if(encrypt_flag){
        set_up_encryption();
    }

    set_terminal_mode();

    /*** Create a socket ***/
    struct sockaddr_in server_address;
    struct hostent* server;

    // create socket
    if((sock_ret = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        print_error("Client: failed to create socket.");
    }

    // get hostname
    server = gethostbyname("localhost");
    if(server == NULL) {
        print_error("Failed to get host by name - no such host.");
    }

    memset((char *)&server_address, 0, sizeof(server_address)); 
    server_address.sin_family = AF_INET;

    memcpy((char *)&server_address.sin_addr.s_addr, (char *)server->h_addr, server->h_length);
    server_address.sin_port = htons(portnum);

    // connect to server
    if(connect(sock_ret, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        print_error("Client: failed to connect.");
    }

    struct pollfd pollfd_descriptors[] = {
        {STDIN_FILENO, POLLIN, 0}, // descriptor for keyboard (stdin)
        {sock_ret, POLLIN, 0} // socket
    };

    while(1) {
        int poll_result = poll(pollfd_descriptors, 2, 0);
        if(poll_result < 0) {
            print_error("Failed to poll.");
        }
        if(poll_result == 0)
            continue;
        short terminal_poll = pollfd_descriptors[0].revents;
        short socket_poll = pollfd_descriptors[1].revents;

        // check terminal readin
        if(terminal_poll == POLLIN) {
            char read_buff[bufsize];
            int readin = read(STDIN_FILENO, &read_buff, bufsize);
            for(i = 0; i < readin; i++) {
                if(read_buff[i] == CARRIAGE_RETURN || read_buff[i] == LINE_FEED) {
                    if(write(STDOUT_FILENO, crlf, 2) < 0) {
                        print_error("Failed to write to terminal. [CR/LF]");
                    }
                } else {
                    if(write(STDOUT_FILENO, (read_buff + i), 1) < 0) {
                        print_error("Failed to write to terminal. [Normal characters]");
                    }
                }

                if(encrypt_flag) { 
                    if(mcrypt_generic(encrypt_fd, read_buff + i, 1) != 0) {
                        print_error("Failed to encrypt.");
                    }
                }
                if(write(sock_ret, read_buff + i, 1) < 0) {
                    print_error("Failed to write to socket file descriptor.");
                }

                // write to log
                if(log_flag) {
                    char log_msg[SENT] = "SENT 1 bytes: ";
                    write(log_fd, log_msg, SENT);
                    write(log_fd, read_buff + i, 1);
                    write(log_fd, "\n", 1);
                }
            }
        } else if(terminal_poll == POLLERR) {
            print_error("Failed to poll STDIN.");
        }

        // read from server
        if(socket_poll == POLLIN){
            int j;
            char read_buff_s[bufsize];
            int readin_s = read(sock_ret, &read_buff_s, bufsize);
            if(readin_s == 0){
                break;
            }

            // decrypt
            if(readin_s < 0) {
                print_error("Failed to read from socket.");
            }
            for(j = 0; j < readin_s; j++) {
                if(log_flag) {
                    char log_msg[RECEIVED] = "RECEIVED 1 bytes: ";
                    write(log_fd, log_msg, RECEIVED);
                    write(log_fd, read_buff_s + j, 1);
                    write(log_fd, "\n", 1);
                }
                if(encrypt_flag) {
                    mdecrypt_generic(decrypt_fd, read_buff_s + j, 1);
                }
                char current_val = read_buff_s[j];
                if(current_val == 0x0D || current_val == 0x0A) {
                    if(write(STDOUT_FILENO, crlf, 2) < 0) {
                        print_error("Failed to write to STDOUT. [1]");
                    }
                } else {
                    if(write(STDOUT_FILENO, read_buff_s + j, 1) < 0) {
                        print_error("Failed to write to STDOUT. [2]");
                    }
                }
            }
        }
        if(socket_poll == POLLHUP || socket_poll == POLLERR) {
            print_error("Error in socket.");
        }
    }

    restore_terminal();
    exit(0);
}
