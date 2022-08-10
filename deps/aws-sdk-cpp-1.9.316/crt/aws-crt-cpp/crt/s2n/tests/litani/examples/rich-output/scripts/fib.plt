set term svg
set style fill solid border
set xlabel "nth fibonacci number"
set ylabel "value"
plot "fib.csv" with linespoints
