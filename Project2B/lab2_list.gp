#! /usr/bin/gnuplot
#
#NAME: Austin Zhang
#EMAIL: aus.zhang@gmail.com
#ID: 604736503
#
# purpose:
#	 generate data reduction graphs for the multi-threaded list project
#
# input: lab2_list.csv
#	1. test name
#	2. # threads
#	3. # iterations per thread
#	4. # lists
#	5. # operations performed (threads x iterations x (ins + lookup + delete))
#	6. run time (ns)
#	7. run time per operation (ns)
#


# general plot parameters
set terminal png
set datafile separator ","

# lab2b_1.png
set title "Throughput vs Number of Threads"
set xlabel "Threads"
set logscale x 2
set xrange[0.75:]
set ylabel "Throughput(ops/s)"
set logscale y 10
set output 'lab2b_1.png'

# grep out only single threaded, un-protected, non-yield results
plot \
    "< grep 'list-none-s,[0-9]*,1000,1,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	    title 'list w/spin' with linespoints lc rgb 'red', \
    "< grep 'list-none-m,[0-9]*,1000,1,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	    title 'list w/mutex' with linespoints lc rgb 'green'

# lab2b_2.png
set title "Mean Mutex Wait and Mean Time per Operation"
set xlabel "Threads"
set logscale x 2
set xrange [0.75:]
set ylabel "Mean Time per Operation"
set logscale y 10
set output 'lab2b_2.png'

# grep out only single threaded, un-protected, non-yield results
plot \
    "< grep 'list-none-m,[0-9]*,1000,1,' lab2b_list.csv" using ($2):($7) \
	    title 'completion time' with linespoints lc rgb 'red', \
    "< grep 'list-none-m,[0-9]*,1000,1,' lab2b_list.csv" using ($2):($8) \
	    title 'wait-for-lock time' with linespoints lc rgb 'green'

# lab2b_3.png
set title "Successful Iterations vs Threads for Each Method"
set logscale x 2
set xlabel "Threads"
set ylabel "successful Iterations"
set logscale y 10
set output 'lab2b_3.png'
plot \
   "< grep list-id-none lab2b_list.csv" using ($2):($3) \
	title 'Unprotected'with points lc rgb 'red', \
    "< grep list-id-s lab2b_list.csv" using ($2):($3) \
	title 'Spin-Lock' with points lc rgb 'green', \
    "< grep list-id-m lab2b_list.csv" using ($2):($3) \
    title 'Mutex' with points lc rgb 'blue' 


# lab2b_4.png
set title "Scalability of Mutex Synchronized Parition Lists"
set xlabel "Threads"
set logscale x 2
set xrange [0.75:]
set ylabel "Throughput(ops/s)"
set logscale y 10
set output 'lab2b_4.png'
plot \
    "< grep 'list-none-m,[0-9]*,1000,1,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	    title 'lists=1' with linespoints lc rgb 'red', \
	"< grep 'list-none-m,[0-9]*,1000,4,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	    title 'lists=4' with linespoints lc rgb 'green', \
	"< grep 'list-none-m,[0-9]*,1000,8,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	    title 'lists=8' with linespoints lc rgb 'blue', \
    "< grep 'list-none-m,[0-9]*,1000,16,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	    title 'lists=16' with linespoints lc rgb 'purple'

# lab2b_5.png
set title "Throughput vs Threads for Spin-Lock-Synchronized Partitioned Lists"
set xlabel "Threads"
set logscale x 2
set xrange [0.75:]
set ylabel "Throughput (ops/sec)"
set logscale y 10
set output 'lab2b_5.png'
plot \
    "< grep 'list-none-s,[0-9]*,1000,1,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	    title 'lists=1' with linespoints lc rgb 'red', \
	"< grep 'list-none-s,[0-9]*,1000,4,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	    title 'lists=4' with linespoints lc rgb 'green', \
	"< grep 'list-none-s,[0-9]*,1000,8,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	    title 'lists=8' with linespoints lc rgb 'blue', \
    "< grep 'list-none-s,[0-9]*,1000,16,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	    title 'lists=16' with linespoints lc rgb 'purple'