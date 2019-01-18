#NAME: Austin Zhang
#EMAIL: aus.zhang@gmail.com
#ID: 604736503

build: 
	gcc -Wall -Wextra -g -o lab2_list -lpthread lab2_list.c SortedList.c
	gcc -Wall -Wextra -g -o lab2_add -lpthread lab2_add.c

tests: build tests.sh
	 @chmod +x tests.sh
	 -./tests.sh
graphs: tests
	./lab2_add.gp
	./lab2_list.gp

clean:
	rm -f lab2_list lab2_add lab2a-604736503.tar.gz

dist: graphs
	tar -czvf lab2a-604736503.tar.gz lab2_add.c lab2_list.c \
	SortedList.h SortedList.c Makefile lab2_add.csv lab2_list.csv \
	lab2_add-1.png lab2_add-2.png lab2_add-3.png lab2_add-4.png \
	lab2_add-5.png lab2_list-1.png lab2_list-2.png lab2_list-3.png \
	lab2_list-4.png README lab2_add.gp lab2_list.gp tests.sh