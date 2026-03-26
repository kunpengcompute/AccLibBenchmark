script_dir=$(dirname "$(realpath "${BASH_SOURCE[0]}")")

perf script -i ./$1 > ./$1.perf

${script_dir}/stackcollapse-perf.pl ./$1.perf > ./$1.folded

${script_dir}/flamegraph.pl ./$1.folded > ../$1.svg

# cat ./$2.folded \
# | sed 's/m-threadpool-[0-9]*/m-threadpool-*/g' \
# | sed 's/ps-io-[0-9]*/ps-io-*/g' \
# | sed 's/ecs-pool-[0-9]*/ecs-pool-*/g' \
# | sed 's/io-[0-9]*/io-*/g' \
# | sed 's/worker-pri3-[0-9]*/worker-pri3-*/g' \
# | sed 's/ps-io-\*\%d[0-9]*/ps-io-*/g' \
# | sed 's/ps-worker-\%d[0-9]*/ps-worker-*/g' \
# | sed 's/ds-io-\*\%d[0-9]*/ds-io-*/g' \
# | sed 's/ds-worker-\%d[0-9]*/ds-worker-*/g' \
# | ./flamegraph.pl --color=hot --countname=Samples --title="Marine Server OnCPU" > ../$2_onFlame.svg
