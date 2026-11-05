# MeowOS 项目约定

RISC-V64 OS，OpenSBI 引导，单核 SV39，单核无 SMP

目录：`src/kernel/`（内核）、`src/user/`（用户态）、`build/`（产物）。

## 命名

- 结构体：`PascalCase` — `Process`, `TrapFrame`, `File`, `Inode`, `FileOP`
- 函数：`snake_case` — `create_user_process`, `find_ready_process`, `process_prepare_return_user`
- 全局变量：`snake_case` — `current_process`, `process_list`, `boot_info`
- 宏 / 枚举成员：`SCREAMING_SNAKE_CASE` — `PROCESS_MAX_COUNT`, `TRAPFRAME_VADDR`, `SYS_COUNT`
- 类型别名：`_t` 后缀（`pt_t`）或 `u8/u16/u32/u64/i64/size_t` 前缀
- 调试函数：`xxx_debug` — `process_debug`, `trap_frame_debug`

## 格式

- 4 空格缩进，不用 tab
- 指针星号靠变量：`Process *p`，**不写** `Process* p`
- 控制语句关键字后留空格：`if (x)`, `while (1)`, `for (int i = 0; ...)`, `return (u64)-1`
- 函数名后不留空格：`foo()`，但定义时 `void foo(void)` 或 `void foo()` 都行（跟随同文件已用的风格）
- K&R 大括号：控制结构 `{` 同行，函数定义 `{` 下一行
- 一行一语句

## 注释

- **中文注释**
- 单行用 `//`，多行/块用 `/* */`
- 非显然的分支、寄存器偏移、魔数必须注释
- 反例：不要写 `/* 保存寄存器 */` 这种把指令复述一遍的注释

## 类型

- 只用 `kernel/lib/typedef.h` 里的 `u8/u16/u32/u64/i64/size_t`，**不直接写** `unsigned long` / `uint64_t`
- 返回值有错误可能时用 `i64`；成功返回 0/true/指针，失败返回 `(u64)-1` / false / NULL
- 结构体字段顺序：先大字段后小字段（节省 padding），但可读性优先

## 错误处理

- 不变量用 `ASSERT(cond)` 保护，不要在 ASSERT 之后假设它能通过
- 致命错误用 `PANIC("func: 什么错, 关键值 = %lx", val)`，**消息带上下文**
- 多步资源分配失败用 `goto` 统一清理，标签按「反序释放」排列：
  ```c
  pt_t page_table = user_alloc_page_table();
  if (!page_table) goto page_table_free;
  if (!mappages(page_table, ...)) goto trampoline_free;
  ...
  trampoline_free:
      user_unmap_pages(page_table, TRAMPOLINE_VADDR, 1, false);
  page_table_free:
      if (page_table) free_walk(page_table, 2);
      return NULL;


## 项目概述

RISC-V64 玩具 OS，OpenSBI 引导，单核 SV39，架构对标 xv6-riscv，nob构建系统，单个文件实现通用构建方案。
不是传统的构建系统，该构建系统使用 nob ，构建脚本放在 `build/src` 下

目录：`src/kernel/`（内核）、`src/user/`（用户态）。

用户态独立编译（`src/user/user.ld` 入口 `0x4000`），通过 `.S` 嵌入内核镜像。

目标：**做到 多进程 + 内存文件系统 + shell 管道** 收工

## 当前状态：能跑什么

- ✅ 内核启动、页表初始化、trampoline、trapframe
- ✅ 用户态 `sprintf`/`printf`、`write`/`read`/`fork`/`exit` syscall 框架
- ✅ 单进程进入用户态、syscall 往返、timer 中断
- ✅ `fork` 部分工作：父进程能打印，子进程能打印，父子 trapframe/页表/栈已复制
- ✅  `wait`/`wakeup`/`kexit`

## 关键设计决策

| 项 | 约定 |
|---|---|
| 系统调用号 | `SYSCALL_LIST(X)` 宏，显式编号，两端各自展开 + `_Static_assert` 防漂移 |
| 用户 ABI | `a0~a5` 参数，`a7` 号，`a0` 返回 |
| 内核取参 | 只用 `get_arg(n)`，不直接读 trapframe 字段 |
| 页大小 | `PAGE_SIZE`，不硬编码 4096 |
| 用户指针 | 永不信，先 validate，再 `copyin/copyout` 逐页 |
| `kernel_trap_entry` | 内核态 trap，只处理 timer，其他 `PANIC` |
| `user_trap_handler` | 用户态 trap，`stvec` 进 trampoline 后切换 |
## 调试技巧

- 崩溃看四件套：`scause → sepc → stval → sstatus`
- `scause=0xf` store page fault，`0xd` load page fault，`0xc` instruction page fault，`0x8` syscall，`0x8000...0005` timer
- `stval` = 出错地址，`sepc` = 出错 PC
- QEMU 里 `info registers` 看完整现场
- `nm build/bin/user.elf | grep 符号名` 确认符号存在
- 如果需要编写测试脚本请使用fish语言，需要通知用户在script中编写了什么脚本

## 给下一个 AI 的话

用户一个人从零搭了这套 OS，**心智模型已经相当扎实**，能读懂 `scause`/`stval`/`sepc`、能定位 trampoline 和页表问题。讨论时可以直接用寄存器现场、`objdump`、日志分析，不需要过度解释基础概念。

用户**最怕的是「没有终点线」和「反复卡在同一个坑」**。给方案时优先给**最小可验证的下一步**（比如「先跑 `wait`，看父进程是否被唤醒」），而不是大而全的重构。用户的调试耐力很好，但需要明确的收敛信号。用户总是对自己的能力不相信，但是这是MIT的研究生大作业并且还是从0开发，一个人，基本从不使用 vibe-coding，用户的能力其实已经超过绝大部分学生。

祝下一个接手者顺利。项目已经过了最难的骨架期，剩下是往 L3 砌砖。

