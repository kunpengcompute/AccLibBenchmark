#export LD_LIBRARY_PATH=/home/z30065763/install/folly-final-final/lib
export LD_LIBRARY_PATH=/home/xxx/ins/folly_POC/lib/
# perf record --call-graph=dwarf -F 999 \
numactl -N 1 ./net-client-ori  --conn_per_thread $1 --qps_per_conn $2 --batch_size $3 --total_requests $4 --server_ip "141.61.11.190" --server_port 10086

# perf script > out.perf
# ../flame/stackcollapse-perf.pl out.perf > out.folded
# ../flame/flamegraph.pl out.folded > Epoll_$1_$2_$3_$4.svg
