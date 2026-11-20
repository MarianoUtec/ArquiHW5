# Quicksort: {6,4,3,2,1,8,9} en 0x1000 debe quedar {1,2,3,4,6,8,9}.
break 0x48
run
expect words 0x1000 6 4 3 2 1 8 9
delete
run
expect words 0x1000 1 2 3 4 6 8 9
expect exit 0
