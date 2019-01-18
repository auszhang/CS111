# NAME: Austin Zhang
# EMAIL: aus.zhang@gmail.com
# ID: 604736503

.SILENT:

default: 
	gcc -o lab4c_tcp -lm -lmraa -Wall -Wextra -g lab4c_tcp.c
	gcc -o lab4c_tls -lm -lmraa -Wall -Wextra -g -lssl -lcrypto lab4c_tls.c

clean:
	rm -f lab4c-604736503.tar.gz lab4c_tcp lab4c_tls

dist:
	tar -czvf lab4c-604736503.tar.gz lab4c_tcp.c lab4c_tls.c Makefile README