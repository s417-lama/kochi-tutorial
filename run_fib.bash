#!/bin/bash
set -euo pipefail

echo "KOCHI_INSTALL_PREFIX_MASSIVETHREADS=$KOCHI_INSTALL_PREFIX_MASSIVETHREADS"

PREFIX=$KOCHI_INSTALL_PREFIX_MASSIVETHREADS
g++ fib.cpp -o fib -O3 -I${PREFIX}/include -L${PREFIX}/lib -Wl,-R${PREFIX}/lib -lmyth

# to check if jemalloc is loaded
export MALLOC_CONF=stats_print:true

./fib $@
