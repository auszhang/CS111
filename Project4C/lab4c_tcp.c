/* 
 * Name: Austin Zhang
 * Email: aus.zhang@gmail.com
 * ID: 604736503
 */

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <poll.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include <unistd.h>
#include <mraa.h>
#include <mraa/aio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

#define A0 1
#define NOTIFY_SERVER 1

int period_arg = 1;
int report = 1;
char scale_arg = 'F';

time_t next_time = 0;
struct tm *cur_time;
struct timeval clk;
FILE *file = 0;

mraa_aio_context temp_read; 

int port = -1;
int sock;
char* id = "";
char* host = "";

struct hostent *server;
struct sockaddr_in server_addr;


void print_error(char* message) {
	fprintf(stderr, message);
	exit(1);
}

// outputs to stdout and log file
void output(char *val, int notify_server) {
	if (notify_server) {
		dprintf(sock, "%s\n", val);
	}
	fprintf(file, "%s\n", val);
	fprintf(stderr, "%s\n", val);
	fflush(file);
}

// get temperature now as a double(self-expanatory)
double get_temperature() {
	int b_val = 4275; // b value of thermister
	float R0 = 100000.0; // 100k
	int temperature = mraa_aio_read(temp_read);
	float R = 1023.0/((float) temperature) - 1.0;
	R = R * R0;
	float ret = 1.0/(log(R/R0)/b_val + 1/298.15) - 273.15; // temperature conversion, from datasheet
	return scale_arg == 'C' ? ret : (ret * 9)/5 + 32; 
}

// outputs a report containing a timestamp of the current time and current temperature
void output_time_temp() {
	if (gettimeofday(&clk, 0) < 0) {
		print_error("Error: could not get time of day.\n");
	}
	if (report && clk.tv_sec >= next_time) {
		double temp = get_temperature();
		int t = temp * 10;
		cur_time = localtime(&clk.tv_sec);
		char out[200];
		sprintf(out, "%02d:%02d:%02d %d.%1d", 
			cur_time->tm_hour, 
			cur_time->tm_min, 
			cur_time->tm_sec, 
			t/10, 
			t%10);
		output(out, NOTIFY_SERVER);
		next_time = clk.tv_sec + period_arg; 
	}
}

// shuts down with timestamp
void shutdown_time(){
	cur_time = localtime(&clk.tv_sec);
	char a[200];
	sprintf(a, "%02d:%02d:%02d SHUTDOWN", 
		cur_time->tm_hour, 
    	cur_time->tm_min, 
    	cur_time->tm_sec);
    output(a, NOTIFY_SERVER);
   	exit(0);
}

// process input from stdin
void handle_input(char *input) {
	while(*input == '\t' || *input == ' ') {
		input++;
	}
	char *period_string = strstr(input, "PERIOD=");
	char *log_string = strstr(input, "LOG");

	if(strcmp(input, "SCALE=F") == 0) {
		output(input, 0);
		scale_arg = 'F'; 
	} else if(strcmp(input, "SCALE=C") == 0) {
		output(input, 0);
		scale_arg = 'C'; 
	} else if(strcmp(input, "START") == 0) {
		output(input, 0);
		report = 1;
	} else if(strcmp(input, "STOP") == 0) {
		output(input, 0);
		report = 0;
	} else if(strcmp(input, "OFF") == 0) {
		output(input, 0);
		shutdown_time();
	} else if(period_string == input) {
		char *in = input;
		in = in + 7; 
		if(*in != 0) {
			int p = atoi(in);
			while(*in != 0) {
				if (!isdigit(*in)) {
					return;
				}
				in++;
			}
			period_arg = p;
		}
		output(input, 0);
	} else if (log_string == input) {
		output(input, 0); 
	}
}

// take input from server
void server_input(char* input) {
	int readin = read(sock, input, 256);
	if (readin > 0) {
		input[readin] = 0;
	}
	char *in = input;
	while (in < &input[readin]) {
		char* cur = in;
		while (cur < &input[readin] && *cur != '\n') {
			cur++;
		}
		*cur = 0;
		handle_input(in);
		in = &cur[1];
	}
}

void parse_options(int argc, char* argv[]) {
	struct option options[] = {
		{"period", required_argument, NULL, 'p'},
		{"scale", required_argument, NULL, 's'},
		{"log", required_argument, NULL, 'l'},
		{"id", required_argument, NULL, 'i'},
		{"host", required_argument, NULL, 'h'},
		{0, 0, 0, 0}
	};

	int opt; 

	while ((opt = getopt_long(argc, argv, "", options, NULL)) != -1) {
		switch (opt) {
			case 'p': 
				period_arg = atoi(optarg);
				break;
			case 's':
				if (optarg[0] == 'F' || optarg[0] == 'C') {
					scale_arg = optarg[0];
					break;
				}
			case 'l':
				file = fopen(optarg, "w+");
            	if (file == NULL) {
	           		print_error("Error: invalid log file.\n");
				}
				break;
			case 'i':
				id = optarg;
				break;
			case 'h':
				host = optarg;
				break;
			default:
				print_error("Invalid options: see usage.\n");
		}
	}
	// get port number
	if (optind < argc) {
		port = atoi(argv[optind]);
		if (port <= 0) {
			print_error("Usage error: port is invalid.\n");
		}
	}
}


int main(int argc, char* argv[]) {
	parse_options(argc, argv);	
	
	if (strlen(id) != 9) {
		print_error("Usage error: use 9 digit ID number.\n");
	} 
	if (strlen(host) == 0) {
		print_error("Usage error: must include argmuement for host.\n");
	}
	if (file == 0) {
		print_error("Usage error: must include argument for log.\n");
	}

	// create a socket
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		print_error("Error: could not create socket in client.\n");
	}
	// find host
	if ((server = gethostbyname(host)) == NULL) {
		fprintf(stderr, "Error: could not find host\n");
	}
	memset((void *) &server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	memcpy((char *) &server_addr.sin_addr.s_addr, (char *) server->h_addr, server->h_length);
	server_addr.sin_port = htons(port);
	if (connect(sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
      	print_error("Error: could not connect on client side (TCP).\n");
  	}

	// send ID terminated by a newline
	char out[50];
	sprintf(out, "ID=%s", id);
	output(out, NOTIFY_SERVER);

	// initialize I/O
	if( (temp_read = mraa_aio_init(A0)) == NULL) {
		fprintf(stderr, "Error: could not initialize AIO.\n");
		mraa_deinit();
		return EXIT_FAILURE;
	}

	struct pollfd poll_in; 
	poll_in.events = POLLIN;
	poll_in.fd = sock; 

	char *input;
	if ((input = (char *)malloc(1024 * sizeof(char))) == NULL) {
		print_error("Error: failed to allocate memory for input buffer.\n");
	}

	while(1) {
		output_time_temp();
		int result = poll(&poll_in, 1, 0);
		if(result) {
			server_input(input);
		}
	}

	mraa_aio_close(temp_read);
	exit(0);
}