# README.md

```markdown
# MeowOS

一个面向 RISC-V64（Sv39）的教学用类 Unix 内核，运行于 QEMU 的 `virt` 机器上，
通过 OpenSBI（`fw_jump`）以 S 态启动。


## 依赖

- `clang`（交叉编译 RISC-V64）
- `gcc`（用于宿主侧构建脚本 / 链接）
- `qemu-system-riscv64`
- OpenSBI 固件 `fw_jump.bin`（`build/bin/mmode.bin` 为其软链接）

## 构建

```bash
# 使用项目自带的 nob 构建系统
gcc build/src/main.c -o build/main
./build/main
```

产物位于 `build/bin/`：

```
build/bin/kernel.elf   内核
build/bin/user.elf     用户程序
build/bin/user.bin     用户程序镜像（被嵌入内核）
build/bin/mmode.bin    OpenSBI 固件
```

## 运行

```bash
./build/main run
```
## 目录结构

```
src/kernel/
├── arch/       启动入口、trap、trampoline、定时器、SBI 封装
├── device/     FDT 解析
├── fs/         文件抽象
├── lib/        printk、string、链表、位图、基础类型
├── mm/         页表、物理页分配
├── proc/       进程、调度、上下文切换、自旋锁
├── syscall/    系统调用分发
└── user/       用户态运行时（user_main、user_start）
```
