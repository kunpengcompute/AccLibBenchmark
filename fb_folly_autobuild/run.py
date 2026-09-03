#!/usr/bin/env python3

import csv
import itertools
import re
import subprocess
import time

# Server sample: numactl -N 0 /home/a00970558/temp_/fbthrift_folly_benchmark/build/press_server --port 23456 --use_io_uring=true --num_cpu_threads=48 --num_io_threads=48


# ==================== 1. Configuration Setup ====================

OUTPUT_CSV = "benchmark_results.csv"
EXECUTABLE = "/home/a00970558/newtemp_/fbthrift_folly_benchmark/build/press_client"
DRY_RUN = False

# Skip prefix runs.
SKIP_RUNS = 0

# [Prefix Group] Fixed command prefix and arguments
PREFIX_CMD = [
    "numactl",
    "-N",
    "0",
    EXECUTABLE,
    "--mode=close_loop",
    "--host=192.168.17.202",
    "--port=23456",
    "--transport=rocket",
    "--test_seconds=90",
    "--compression=none",
    "--rpc_timeout_ms=30000",
    "--echo_attachment=false",
    "--use_io_uring=false",
    "--random_fields=true",
]

# [Conn Setup Group] [cpu_threads, io_threads, connections]
CONN_SETUPS = [
    [1, 1, 50],
    [10, 10, 500],
    [20, 20, 1000],
    [40, 40, 1000],
]

# [QD Group] Queue Depth
QD_SETUPS = [10, 25]

# [Data Setup] list[list[str, list[str]]]: Name in CSV, Exec Arguments
DATA_SETUPS = [
    ["binary payload", ["--fields=none", "--payload_size=256"]],
    ["binary payload", ["--fields=none", "--payload_size=1024"]],
    ["binary payload", ["--fields=none", "--payload_size=4096"]],
    ["binary payload", ["--fields=none", "--payload_size=8192"]],
    ["binary payload", ["--fields=none", "--payload_size=16384"]],
    ["i32 int", ["--fields=int_val", "--payload_size=2048"]],
    ["i64 long", ["--fields=long_val", "--payload_size=1024"]],
    ["double", ["--fields=double_val", "--payload_size=512"]],
    ["list<i32>", ["--fields=int_list", "--int_list_size=32", "--payload_size=4096"]],
    ["list<i64>", ["--fields=i64_list", "--int_list_size=32", "--payload_size=2048"]],
    ["string", ["--fields=str_val", "--payload_size=8192"]],
    ["list<string>", ["--fields=str_list", "--str_list_size=32", "--payload_size=16384"]],
    ["set<string>", ["--fields=str_set", "--payload_size=16384"]],
    ["map<string, i64>", ["--fields=kv_map", "--kv_map_size=32", "--payload_size=4096"]],
]

# ==================== 2. Output Parsing ====================


def parse_output(output_text: str) -> dict[str, str]:
    """Extract Payload (numeric only), Throughput, Latency, and compute Success Rate."""
    data = {}

    # Extract Payload digits only (e.g., "1024B" -> "1024")
    payload_match = re.search(r"Payload:\s*(\d+)", output_text)
    data["payload"] = payload_match.group(1) if payload_match else "N/A"

    # Extract Throughput (e.g., 1330.80MB/s)
    tp_match = re.search(r"Throughput:\s*([\d\.]+MB/s)", output_text)
    data["Throughput"] = tp_match.group(1) if tp_match else "N/A"

    # Extract Latency: avg and p99 (numeric only)
    lat_match = re.search(
        r"Latency \(us\):.*?avg=([\d\.]+).*?p99=([\d\.]+)", output_text
    )
    if lat_match:
        data["latencyAvg"] = lat_match.group(1)
        data["latencyP99"] = lat_match.group(2)
    else:
        data["latencyAvg"] = "N/A"
        data["latencyP99"] = "N/A"
    
    server_cpu_match = re.search(r"Server CPU \(%\):.*?avg=([\d\.]+)", output_text)
    if server_cpu_match:
        data["Server CPU (%)"] = f"{float(server_cpu_match.group(1)):.2f}"
    else:
        data["Server CPU (%)"] = "N/A"

    client_cpu_match = re.search(r"Client CPU \(%\):.*?avg=([\d\.]+)", output_text)
    if client_cpu_match:
        data["Client CPU (%)"] = f"{float(client_cpu_match.group(1)):.2f}"
    else:
        data["Client CPU (%)"] = "N/A"
    
    # Extract QPS & calculate Success Rate
    qps_match = re.search(r"QPS:\s*([\d\.]+)", output_text)
    succ_match = re.search(r"Success:\s*(\d+)", output_text)
    fail_match = re.search(r"Fail:\s*(\d+)", output_text)

    if qps_match and succ_match and fail_match:
        qps_str = qps_match.group(1)
        success = int(succ_match.group(1))
        fail = int(fail_match.group(1))
        total = success + fail
        success_rate = (success / total * 100) if total > 0 else 0.0
        data["QPS/成功率"] = f"{qps_str}/{success_rate:.2f}%"
    else:
        data["QPS/成功率"] = "N/A"

    return data


# ==================== 3. Benchmark Matrix & CSV Output ====================


def run_benchmark_matrix() -> None:
    headers = [
        "cpu_thread",
        "io_thread",
        "queue_dep",
        "connection",
        "payload",
        "data_types",
        "Throughput",
        "latencyAvg",
        "latencyP99",
        "QPS/成功率",
        "Client CPU (%)",
        "Server CPU (%)"
    ]

    with open(OUTPUT_CSV, mode="w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(headers)

        # Order: DATA_SETUPS -> CONN_SETUPS -> QD_SETUPS
        matrix = itertools.product(DATA_SETUPS, CONN_SETUPS, QD_SETUPS)
        total_case = len(DATA_SETUPS) * len(CONN_SETUPS) * len(QD_SETUPS)

        for i, ((data_type, cli_args), conn_item, qd) in enumerate(matrix):

            # Skip prefix runs designated at premble section.
            if (i < SKIP_RUNS):
                continue

            # Explicit indexing from list[int]: [cpu_threads, io_threads, connections]
            cpu_t, io_t, conn = conn_item[0], conn_item[1], conn_item[2]

            # Assemble dynamic arguments
            dynamic_args = [
                f"--cpu_threads={cpu_t}",
                f"--io_threads={io_t}",
                f"--connections={conn}",
                f"--queue_depth={qd}",
            ] + cli_args

            full_cmd = PREFIX_CMD + dynamic_args
            print(f"\n[{i+1}/{total_case}] {' '.join(full_cmd)}")

            if not DRY_RUN:
                # Execute command and capture output directly
                try:
                    res = subprocess.run(
                        full_cmd,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT,
                        text=True,
                        check=True,
                    )
                    raw_log = res.stdout
                except subprocess.CalledProcessError as e:
                    print(f"[Error] Command failed with code {e.returncode}")
                    raw_log = e.output if e.output else ""

                # Parse metrics
                metrics = parse_output(raw_log)
                if "N/A" in metrics.values():
                    print("[Warning] Some metrics could not be parsed:")
                    print(raw_log)

                # Construct row
                row = [
                    cpu_t,
                    io_t,
                    qd,
                    conn,
                    metrics["payload"],  # Pure numbers (e.g., 1024)
                    data_type,
                    metrics["Throughput"],
                    metrics["latencyAvg"],
                    metrics["latencyP99"],
                    metrics["QPS/成功率"],
                    metrics["Client CPU (%)"],
                    metrics["Server CPU (%)"]
                ]

                writer.writerow(row)
                f.flush()
                time.sleep(5)


    print(f"\n[Finished] Results saved to {OUTPUT_CSV}")


if __name__ == "__main__":
    run_benchmark_matrix()
