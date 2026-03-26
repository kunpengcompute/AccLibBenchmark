import subprocess
import os
import argparse
import re


NUM_THREADS = 1
BATCH_SIZE = 1


def execute_command(cmd: str) -> str:
    """
    执行单条 shell 命令并返回终端打印的所有内容。
    """
    try:
        result = subprocess.run(
            cmd,
            shell=True,
            capture_output=True,
            text=True,
            encoding='utf-8'  # 显式指定编码防止 Windows 下乱码
        )

        # 合并输出内容（包含正常打印和错误打印）
        output = result.stdout + result.stderr
        return output.strip()

    except Exception as e:
        return f"系统级错误: {str(e)}"


def check_validation():
    current_file = os.path.abspath(__file__)
    current_dir = os.path.abspath(os.path.dirname(current_file))
    all_folders = os.listdir(current_dir)
    assert "epoll" in all_folders
    assert "iouring" in all_folders

    epoll_dir = os.path.join(current_dir, "epoll")
    iouring_dir = os.path.join(current_dir, "iouring")

    epoll_files = os.listdir(epoll_dir)
    iouring_files = os.listdir(iouring_dir)

    if "net-client-ori" not in epoll_files:
        os.chdir(epoll_dir)
        execute_command("make")

    if "net-client" in iouring_files:
        os.chdir(iouring_dir)
        execute_command("make")


def extract_qps(output_text: str, qps_name: str) -> float:
    """
    通用指标提取函数
    :param output_text: 终端输出的完整字符串
    :param qps_name: 要提取的指标名称 (例如 'TARGET' 或 'CURRENT')
    :return: 提取到的浮点数，若未匹配则返回 0.0
    """
    # 构建正则：匹配 qps_name + 任意空格 + QPS: + 任意空格 + 数字
    # re.IGNORECASE 让函数对大小写不敏感（可选）
    pattern = rf"{qps_name}\s+QPS:\s+([\d.]+)"

    match = re.search(pattern, output_text, re.IGNORECASE)

    if match:
        try:
            return float(match.group(1))
        except ValueError:
            return 0.0

    return 0.0


def is_qps_close(target: float, current: float) -> bool:
    """
    判断 current 是否接近 target。
    标准：绝对误差 < 100 且 相对误差 < 1%
    """
    if target == 0:
        return current == 0

    # 1. 计算绝对误差
    abs_diff = abs(target - current)

    # 2. 计算相对误差 (以 target 为基准)
    relative_diff = abs_diff / target

    # 3. 同时满足两个条件
    # 1% 对应 0.01
    is_abs_ok = abs_diff < 100
    is_rel_ok = relative_diff < 0.01

    return is_abs_ok and is_rel_ok


def run_single_test(qps: int, duration: int, max_repeat: int):
    for i in range(max_repeat):
        command = f"bash run.sh {NUM_THREADS} {qps} {BATCH_SIZE} {qps*duration}"
        output = execute_command(command)
        current_qps = extract_qps(output, "CURRENT")
        target_qps = extract_qps(output, "TARGET")
        if is_qps_close(target_qps, current_qps):
            return True
    return False


def run_test(mode: str, duration: int, max_repeat: int):
    current_file = os.path.abspath(__file__)
    current_dir = os.path.abspath(os.path.dirname(current_file))
    if mode == "epoll":
        os.chdir(os.path.join(current_dir, "epoll"))
    if mode == "iouring":
        os.chdir(os.path.join(current_dir, "iouring"))

    start_value = 100000
    right_value = 1000000
    rough_search_result = 0
    for qps in range(start_value, right_value, 100000):
        print(f"rough search {qps}: ", end="")
        result = run_single_test(qps, duration, max_repeat)
        if not result:
            print("no pass")
            rough_search_result = qps
            break
        else:
            print("pass")

    accurate_search_result = 0
    for _qps in range(10000, 100000, 10000):
        print(f"accurate search {rough_search_result - _qps}: ", end="")
        result = run_single_test(rough_search_result - _qps, duration, max_repeat)
        if result:
            print("pass")
            accurate_search_result = _qps
            break
        else:
            print("no pass")
    final_value = rough_search_result - accurate_search_result
    print(f"------Max QPS: {final_value}------")


def parser_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", type=str, choices=["epoll", "iouring", "all"], help="请选择执行epoll还是iouring或者全部")
    parser.add_argument("--duration", type=int, default=1, help="单次测试时长")
    parser.add_argument("--max_repeat", type=int, default=5, help="最大重复测试次数")
    return parser.parse_args()


def main():
    args = parser_args()

    check_validation()
    if args.mode == "all":
        args.mode = "epoll"
        run_test(**vars(args))
        args.mode = "iouring"
    run_test(**vars(args))


if __name__ == "__main__":
    main()
