Usage
----------

channeled all2all async -
  
./test 0 run_iterations routing_table_file flush_size sync_iterations

example:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~bash
>> ./test 0 100 route_table.file 2048 50
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#
all2all half-async

./test 1 run_iterations routing_table_file max_gap packet_size

example:

*note* that the routing file must contain all nodes in each line
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~bash
>> ./test 1 100 route_table.file 5 1000000 
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#
all2all routed-sync 

./test 2 run_iterations routing_table_file packet_size

example:

*note* that the routing file must contain all nodes in each line
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~bash
>> ./test 2 100 route_table.file 1000000
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
#

all2all sync

./test 3 run_iterations min_packet_size max_packet_size

example:  // 100KB - 100MB
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~bash
>> ./test 3 100 100000 100000000    
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#


route_table.file
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~text
Table format:
0: 1, 2, 3
1: 0, 3, 2
2: 3, 1, 0
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
