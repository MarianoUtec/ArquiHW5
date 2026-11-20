# ELF compilado desde C. Entrada para read_int: N = 30.
input 30
break criba
run
expect pc criba
expect a0 30
delete
run
expect output 2 3 5 7 11 13 17 19 23 29
expect words primos 2 3 5 7 11 13 17 19 23 29
expect mem32 total 10
expect exit 0
