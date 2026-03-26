#export LD_LIBRARY_PATH=/home/j00882799/dongming_folly/lib:/root/snappy/build/
# perf record --call-graph=dwarf -F 999 -o ./perf_data/4threads_epoll_$1_$2_$3_$4_allsys.data -ag -i --  \
# numactl -N 1 ./epoll/net-client-ori --conn_per_thread $1 --qps_per_conn $2 --batch_size $3 --total_requests $4 --server_port 10016 &
# numactl -N 1 ./epoll/net-client-ori --conn_per_thread $1 --qps_per_conn $2 --batch_size $3 --total_requests $4 --server_port 10016 &
# numactl -N 1 ./epoll/net-client-ori --conn_per_thread $1 --qps_per_conn $2 --batch_size $3 --total_requests $4 --server_port 10016 &
# numactl -N 1 ./epoll/net-client-ori --conn_per_thread $1 --qps_per_conn $2 --batch_size $3 --total_requests $4 --server_port 10016 & wait

# bash ./perf_data/gen_flamgraph.sh /home/g00570279/iouring_benchmark/code_1111/perf_data 4threads_epoll_$1_$2_$3_$4_allsys

perf record --call-graph=dwarf -F 999 -o ./perf_data/3evb_IOuring_$1_$2_$3_$4_$5allsys.data -ag -i --  \
numactl -N 1 ./iouring/net-client  --conn_per_thread $1 --qps_per_conn $2 --batch_size $3 --total_requests $4 --server_port 10016 --evbs_per_thread $5 > log.txt

bash ./gen_flamgraph.sh /home/g00570279/iouring_benchmark/folly-iouring-bench2/perf_data/perf_data 3evb_IOuring_$1_$2_$3_$4_allsys