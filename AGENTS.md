# 给下一个 AI 的话（第二版）

## 项目现状

RISC-V64 玩具 OS，OpenSBI 引导，单核 SV39，nob 构建（`build/src/main.c`，产物 `build/main`）。
目标：**多进程 + 内存文件系统 + shell 管道**。

### 已完成并验证

- 内核启动、页表、trampoline、trapframe、timer 中断
- 用户态 syscall 框架，`fork/wait/exit/getpid/palloc/pfree` 全绿
- **设备中断**：PLIC（context 1，hart0 S-mode）+ UART0（IRQ 10）已打通，S 模式 trap 里 `dev_interrupt` 认 timer 和外部中断
- **控制台**：`console_intr` 攒行，`console_read` 睡在 `cons.r` 上，`uartintr` → `wakeup`。`cpu is idle` 后能被打字叫醒
- **文件系统**：`inode.c` / `file.c` 拆分完成，`reg_fop`（普通字节文件）、`dir_fop`（目录）、`io_fop`（console）、`pipe_fop`（管道）四个 fop 齐了
- `open/create/unlink/mkdir/dup/dup2/readdir/read/write/close` 全部通过测试（79 条 all green）
- **管道**：`create_pipe_file` + `pipe_read`/`pipe_write`/`pipe_release` 已写完，`PipeData` 按 `in->ino` 索引（不是 pid/fd），跨 fork 共享。测试已写，待跑

### 关键设计决策（不要改）

| 项 | 约定 |
|---|---|
| 系统调用号 | `SYSCALL_LIST(X)` 宏，`_Static_assert` 防漂移；当前到 `UNLINK=15`，`PIPE=16` 已定义待启用 |
| 用户 ABI | `a0~a5` 参数，`a7` 号，`a0` 返回 |
| 内核取参 | 只用 `get_arg(n)` |
| 页大小 | `PAGE_SIZE` |
| 用户指针 | 一律 `copy_from_user`/`copy_to_user`/`copy_str_from_user` 逐页校验，**永不**直接解引用 |
| `FileOp` | `read/write/lseek/readdir/release` 五栏，按 fop 分派，`File` 不存类型 |
| `Inode.ref` | 目录项 + 打开实例，`open_file` 统一 `ref_inode`，`release` 统一 `unref_inode` |
| 管道共享状态 | `PipeData` 按 `in->ino` 索引，字段 `start/end/readers/writers` |
| `sleep` chan | 用 `Inode*` 作身份，不按 fd/pid |
| 目录项 | `DirEntry { char name[24]; Inode *inode; }`，定长数组，`cap` 一页 |
| 系统调用命名 | **只保留 `sys_*`**，`kread/kwrite/kopen/kclose/kunlink` 这些薄壳已删除，逻辑并入 `sys_*` |

### 代码风格

- 4 空格缩进，指针星号靠变量：`Process *p`
- 注释：单行 `//`，多行 `/* */` 包裹
- 结构体 `PascalCase`，函数 `snake_case`，宏 `SCREAMING_SNAKE_CASE`
- 只写 `u8/u16/u32/u64/i64/size_t`，不用 `unsigned long`
- 错误返回：`(u64)-1` / `-1` / `NULL`，成功 `0` / `true` / 指针调试技巧（沿用）
崩溃看四件套：scause → sepc → stval → sstatus

scause=0xd load page fault，0xf store page fault，0xc instruction page fault，0x8 syscall

scause 最高位为 1 是中断，低 5 位是类型：5 timer，9 外部中断

stval=0 + scause=0xc → NULL 函数指针调用，八成是 free_inode 或 op->xxx 没设

MMIO 地址：UART0 0x10000000，PLIC 0x0c000000（映射 4MB），VirtIO 0x10001000。分页开后必须映射，不映射第一次 ld/st 就 fault

QEMU 里 info registers / info mem 看现场

nm build/bin/user.elf | grep 符号名

测试脚本用 fish 写，写完通知用户

给下一个接手者
用户一个人从零搭了这套 OS，xv6 路子的仿制品，MIT 研究生大作业级别，本科生手写，几乎不用 vibe coding。心智模型扎实，能读 scause/stval/sepc，能定位 trampoline 和页表问题。讨论时直接用寄存器现场、objdump、日志分析，不需要解释基础概念。

用户调试耐力很好，但需要明确的收敛信号。

如果用户对「古法编程」产生退缩、想用 AI 生成大段代码代替自己写，直接拿最严厉的语气批评。他坚持手写才是获取真正知识和能力的路，这一点上不含糊。

当前状态：多进程 ✅，内存文件系统 ✅，管道接近 ✅。
剩下 shell 和最后那条 echo hello | cat。骨架期过了，现在是砌砖，每一块都有明确的灰缝——对上就稳。

