# NAME: Austin Zhang
# EMAIL: aus.zhang@gmail.com
# ID: 604736503

default: 
	gcc -Wall -Wextra -lz -o lab1b-client lab1b-client.c -lmcrypt
	gcc -Wall -Wextra -lz -o lab1b-server lab1b-server.c -lmcrypt

client: 
	gcc -Wall -Wextra -lz -o lab1b-client lab1b-client.c

server:
	gcc -Wall -Wextra -lz -o lab1b-server lab1b-server.c

dist: 
	tar -cvzf lab1b-604736503.tar.gz lab1b-server.c lab1b-client.c \
		README Makefile my.key

clean:
	rm -f lab1b-604736503.tar.gz lab1b-client lab1b-server