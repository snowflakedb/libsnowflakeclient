set term svg
set style fill solid border
set xlabel "x"
set ylabel "sin(x)"
plot 0, "sin.csv" smooth csplines
