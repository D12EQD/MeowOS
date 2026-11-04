#!/usr/bin/env bash
#
# 用户态系统调用测试的脚本入口：构建 -> 跑 QEMU -> 过滤结果
# 用法：script/run_test.sh [超时秒数]
#
# 根进程没有父进程，退出会触发内核 PANIC，所以用户程序跑完就空转，
# 只能靠 timeout 结束 QEMU，因此 QEMU 返回 124 是正常现象。
set -uo pipefail

cd "$(dirname "$0")/.." || exit 1

TIMEOUT="${1:-45}"
LOG=build/test_run.log

echo "== build =="
if ! ./build/main > build/test_build.log 2>&1; then
    tail -20 build/test_build.log
    echo "build failed"
    exit 1
fi

echo "== run (timeout ${TIMEOUT}s) =="
timeout "$TIMEOUT" qemu-system-riscv64 \
    -machine virt \
    -bios build/bin/mmode.bin \
    -nographic \
    -serial mon:stdio \
    --no-reboot \
    -kernel build/bin/kernel.elf \
    2>&1 | sed -r 's/\x1B\[[0-9;]*[mK]//g' > "$LOG"

# 内核的 pr_debug 会和用户输出交织，只挑测试相关的行
grep -a "\[ CASE \]\|\[ PASS \]\|\[ FAIL \]\|\[ NOTE \]\|EXPECT>\|ACTUAL>\|TEST SUMMARY" "$LOG"

PASS=$(grep -ac "\[ PASS \]" "$LOG")
FAIL=$(grep -ac "\[ FAIL \]" "$LOG")
SUMMARY=$(grep -a "TEST SUMMARY" "$LOG" | tail -1)

echo
if [ -z "$SUMMARY" ]; then
    echo "!! 测试没有跑完（日志见 $LOG）"
    exit 2
fi

echo "pass=${PASS} fail=${FAIL}"
[ "$FAIL" -eq 0 ] || exit 1
